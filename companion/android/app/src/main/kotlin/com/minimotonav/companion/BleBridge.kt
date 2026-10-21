package com.minimotonav.companion

import android.app.Activity
import android.os.Handler
import android.os.Looper
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel

class BleBridge(private val activity: Activity) :
    MethodChannel.MethodCallHandler,
    EventChannel.StreamHandler,
    BleManager.Listener {

    private val handler = Handler(Looper.getMainLooper())
    private val manager = BleManager(activity.applicationContext, this)
    private var events: EventChannel.EventSink? = null

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "scan" -> {
                if (!manager.hasPermissions()) {
                    activity.requestPermissions(manager.requiredPermissions(), PERMISSION_REQUEST)
                    result.success(false)
                    return
                }
                result.success(manager.startScan())
            }
            "stopScan" -> {
                manager.stopScan()
                result.success(null)
            }
            "connect" -> {
                val address = call.argument<String>("address")
                if (address == null) {
                    result.error("bad_args", "address required", null)
                    return
                }
                if (!manager.hasPermissions()) {
                    activity.requestPermissions(manager.requiredPermissions(), PERMISSION_REQUEST)
                    result.success(false)
                    return
                }
                manager.connect(address)
                result.success(true)
            }
            "disconnect" -> {
                manager.disconnect()
                result.success(null)
            }
            "send" -> {
                val kind = call.argument<String>("kind")
                val bytes = call.argument<ByteArray>("bytes")
                if (kind == null || bytes == null) {
                    result.error("bad_args", "kind and bytes required", null)
                    return
                }
                result.success(manager.write(kind, bytes))
            }
            "requestPermissions" -> {
                activity.requestPermissions(manager.requiredPermissions(), PERMISSION_REQUEST)
                result.success(null)
            }
            "status" -> result.success(
                mapOf(
                    "permissions" to manager.hasPermissions(),
                    "version" to BleProtocol.VERSION,
                ),
            )
            else -> result.notImplemented()
        }
    }

    override fun onListen(arguments: Any?, sink: EventChannel.EventSink?) {
        events = sink
        sink?.success(mapOf("event" to "state", "state" to "idle"))
    }

    override fun onCancel(arguments: Any?) {
        events = null
    }

    override fun onState(state: String, message: String?) {
        handler.post {
            events?.success(
                mapOf("event" to "state", "state" to state, "message" to message),
            )
        }
    }

    override fun onDevice(name: String?, address: String, rssi: Int) {
        handler.post {
            events?.success(
                mapOf(
                    "event" to "device",
                    "name" to name,
                    "address" to address,
                    "rssi" to rssi,
                ),
            )
        }
    }

    override fun onMtu(mtu: Int) {
        handler.post {
            events?.success(mapOf("event" to "mtu", "mtu" to mtu))
        }
    }

    override fun onDropped(total: Int) {
        handler.post {
            events?.success(mapOf("event" to "dropped", "total" to total))
        }
    }

    fun dispose() {
        manager.close()
        events = null
    }

    companion object {
        const val METHOD_CHANNEL = "moto/ble"
        const val EVENT_CHANNEL = "moto/ble/events"
        private const val PERMISSION_REQUEST = 7301
    }
}
