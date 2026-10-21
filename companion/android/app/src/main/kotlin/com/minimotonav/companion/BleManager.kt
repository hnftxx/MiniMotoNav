package com.minimotonav.companion

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import java.util.ArrayDeque
import java.util.UUID
import kotlin.math.min

class BleManager(private val context: Context, private val listener: Listener) {
    interface Listener {
        fun onState(state: String, message: String? = null)
        fun onDevice(name: String?, address: String, rssi: Int)
        fun onMtu(mtu: Int)
        fun onDropped(total: Int)
    }

    private data class PendingWrite(val characteristic: UUID, val bytes: ByteArray)

    private val handler = Handler(Looper.getMainLooper())
    private val adapter: BluetoothAdapter? =
        context.getSystemService(BluetoothManager::class.java)?.adapter
    private val queue = ArrayDeque<PendingWrite>()
    private var scanner: android.bluetooth.le.BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var mtu = 23
    private var busy = false
    private var scanning = false
    private var shouldReconnect = false
    private var reconnectAttempt = 0
    private var pendingAddress: String? = null
    private var dropped = 0
    private var awaitingCccd = false

    private val writeWatchdog = Runnable {
        if (!busy) return@Runnable
        busy = false
        emit("error", "write timed out")
        drain()
    }

    private val reconnect = Runnable {
        pendingAddress?.let { openGatt(it) }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.scanRecord?.deviceName ?: result.device.name
            listener.onDevice(name, result.device.address, result.rssi)
        }

        override fun onScanFailed(errorCode: Int) {
            scanning = false
            emit("error", "scan failed: $errorCode")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                this@BleManager.gatt?.close()
                this@BleManager.gatt = null
                busy = false
                awaitingCccd = false
                handler.removeCallbacks(writeWatchdog)
                queue.clear()
                emit("disconnected", "gatt status $status")
                scheduleReconnect()
                return
            }
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                reconnectAttempt = 0
                emit("connected")
                if (!gatt.requestMtu(BleProtocol.DEFAULT_MTU)) {
                    gatt.discoverServices()
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                this@BleManager.gatt?.close()
                this@BleManager.gatt = null
                busy = false
                awaitingCccd = false
                handler.removeCallbacks(writeWatchdog)
                queue.clear()
                emit("disconnected")
                scheduleReconnect()
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, newMtu: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                mtu = newMtu
                listener.onMtu(mtu)
            }
            gatt.discoverServices()
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                emit("error", "service discovery failed: $status")
                this@BleManager.gatt?.close()
                this@BleManager.gatt = null
                scheduleReconnect()
                return
            }
            val service = gatt.getService(BleProtocol.SERVICE_UUID)
            if (service == null) {
                emit("error", "display service missing")
                return
            }
            val required = listOf(
                BleProtocol.TELEMETRY_UUID,
                BleProtocol.NAVIGATION_UUID,
                BleProtocol.CONTROL_UUID,
                BleProtocol.STATUS_UUID,
            )
            if (required.any { service.getCharacteristic(it) == null }) {
                emit("error", "display characteristics missing")
                return
            }
            val statusChar = service.getCharacteristic(BleProtocol.STATUS_UUID)
            gatt.setCharacteristicNotification(statusChar, true)
            val descriptor = statusChar.getDescriptor(BleProtocol.CCCD_UUID)
            if (descriptor == null) {
                finishReady()
                return
            }
            awaitingCccd = true
            val started = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeDescriptor(
                    descriptor,
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE,
                )
            } else {
                @Suppress("DEPRECATION")
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                gatt.writeDescriptor(descriptor)
            }
            if (!started) {
                awaitingCccd = false
                emit("error", "status notifications unavailable")
                finishReady()
            }
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int,
        ) {
            if (!awaitingCccd || descriptor.uuid != BleProtocol.CCCD_UUID) return
            awaitingCccd = false
            if (status != BluetoothGatt.GATT_SUCCESS) {
                emit("error", "status notifications rejected: $status")
            }
            finishReady()
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            handler.removeCallbacks(writeWatchdog)
            if (status != BluetoothGatt.GATT_SUCCESS) {
                emit("error", "write failed: $status")
            }
            busy = false
            drain()
        }

        @Deprecated("Deprecated in API 33")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
        ) {
            @Suppress("DEPRECATION")
            emitStatus(characteristic, characteristic.value ?: ByteArray(0))
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            emitStatus(characteristic, value)
        }
    }

    private fun emitStatus(characteristic: BluetoothGattCharacteristic, value: ByteArray) {
        if (characteristic.uuid == BleProtocol.STATUS_UUID) {
            emit("status", value.joinToString("") { "%02x".format(it) })
        }
    }

    private fun finishReady() {
        emit("ready")
        listener.onMtu(mtu)
        drain()
    }

    fun hasPermissions(): Boolean {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) ==
                PackageManager.PERMISSION_GRANTED &&
                context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) ==
                PackageManager.PERMISSION_GRANTED
        }
        return context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) ==
            PackageManager.PERMISSION_GRANTED
    }

    fun requiredPermissions(): Array<String> {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
            )
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan(): Boolean {
        if (adapter == null) {
            emit("unsupported", "device has no Bluetooth adapter")
            return false
        }
        if (!adapter.isEnabled) {
            emit("bluetooth_off", "enable Bluetooth to scan")
            return false
        }
        if (!hasPermissions()) {
            emit("permission", "Bluetooth permission is required")
            return false
        }
        if (scanning) return true
        val bleScanner = adapter.bluetoothLeScanner
        if (bleScanner == null) {
            emit("unsupported", "LE scanner unavailable")
            return false
        }
        scanner = bleScanner
        scanning = true
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(BleProtocol.SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setReportDelay(0)
            .build()
        bleScanner.startScan(listOf(filter), settings, scanCallback)
        emit("scanning")
        return true
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (scanning && hasPermissions()) {
            scanner?.stopScan(scanCallback)
        }
        scanning = false
        if (gatt == null) emit("idle")
    }

    @SuppressLint("MissingPermission")
    fun connect(address: String) {
        if (!hasPermissions()) {
            emit("permission", "Bluetooth permission is required")
            return
        }
        stopScan()
        shouldReconnect = true
        pendingAddress = address
        reconnectAttempt = 0
        openGatt(address)
    }

    @SuppressLint("MissingPermission")
    private fun openGatt(address: String) {
        if (!hasPermissions()) return
        val device = adapter?.getRemoteDevice(address) ?: run {
            emit("error", "unknown device")
            return
        }
        gatt?.close()
        mtu = 23
        busy = false
        awaitingCccd = false
        handler.removeCallbacks(writeWatchdog)
        queue.clear()
        emit("connecting")
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    fun disconnect() {
        shouldReconnect = false
        handler.removeCallbacks(reconnect)
        pendingAddress = null
        stopScan()
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        busy = false
        awaitingCccd = false
        handler.removeCallbacks(writeWatchdog)
        queue.clear()
        emit("idle")
    }

    fun close() {
        shouldReconnect = false
        handler.removeCallbacks(reconnect)
        stopScan()
        gatt?.close()
        gatt = null
        busy = false
        awaitingCccd = false
        handler.removeCallbacks(writeWatchdog)
        queue.clear()
    }

    @Synchronized
    fun write(kind: String, bytes: ByteArray): Boolean {
        val characteristic = when (kind) {
            "telemetry" -> BleProtocol.TELEMETRY_UUID
            "navigation" -> BleProtocol.NAVIGATION_UUID
            "control" -> BleProtocol.CONTROL_UUID
            else -> return false
        }
        if (gatt == null) return false
        if (bytes.size > BleProtocol.maxWritePayload(mtu)) return false
        if (queue.size >= 128) {
            queue.removeFirst()
            dropped++
            listener.onDropped(dropped)
        }
        queue.addLast(PendingWrite(characteristic, bytes))
        handler.post { drain() }
        return true
    }

    @SuppressLint("MissingPermission")
    private fun drain() {
        if (busy) return
        val current = gatt ?: return
        val next = synchronized(this) {
            if (queue.isEmpty()) null else queue.removeFirst()
        } ?: return
        val service = current.getService(BleProtocol.SERVICE_UUID) ?: return
        val characteristic = service.getCharacteristic(next.characteristic) ?: return
        busy = true
        handler.removeCallbacks(writeWatchdog)
        handler.postDelayed(writeWatchdog, 5_000L)
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val result = current.writeCharacteristic(
                characteristic,
                next.bytes,
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
            )
            if (result != BluetoothGatt.GATT_SUCCESS) {
                handler.removeCallbacks(writeWatchdog)
                busy = false
                emit("error", "write rejected: $result")
                drain()
            }
        } else {
            @Suppress("DEPRECATION")
            characteristic.value = next.bytes
            @Suppress("DEPRECATION")
            if (!current.writeCharacteristic(characteristic)) {
                handler.removeCallbacks(writeWatchdog)
                busy = false
                emit("error", "write rejected")
                drain()
            }
        }
    }

    private fun scheduleReconnect() {
        if (!shouldReconnect || pendingAddress == null) return
        val delay = min(30_000L, 1_000L shl min(reconnectAttempt, 5))
        reconnectAttempt++
        handler.removeCallbacks(reconnect)
        handler.postDelayed(reconnect, delay)
    }

    private fun emit(state: String, message: String? = null) {
        handler.post { listener.onState(state, message) }
    }
}
