package com.minimotonav.companion

import com.minimotonav.protocol.DecodedFrame
import com.minimotonav.protocol.FrameCodec
import com.minimotonav.protocol.FrameParser
import com.minimotonav.protocol.Objects
import com.minimotonav.protocol.Operation
import com.minimotonav.protocol.Parameter
import com.minimotonav.protocol.TelemetryAssembler
import com.minimotonav.protocol.TelemetrySample
import com.minimotonav.protocol.assertOperation
import com.minimotonav.protocol.deviceNameFrame
import com.minimotonav.protocol.identityFrame
import com.minimotonav.protocol.isSdoReplyId
import com.minimotonav.protocol.isTelemetryId
import com.minimotonav.protocol.subscribeAllFrames
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import java.util.concurrent.atomic.AtomicBoolean

fun interface SocketBinder {
    fun bind(socket: Socket)
}

/**
 * TCP transport and app-profile session.
 *
 * Read-only by construction: every transmit passes through assertOperation, so
 * a caller cannot send a tuning/calibration write to a real ECU. There is no
 * bypass parameter.
 */
class AracerClient(
    val host: String = DEFAULT_HOST,
    val port: Int = DEFAULT_PORT,
    private val seq: Int = 0,
    private val connectTimeoutMs: Int = CONNECT_TIMEOUT_MS,
    resyncOnHeader: Boolean = false,
    private val binder: SocketBinder? = null,
    private val listener: Listener,
) {
    interface Listener {
        fun onConnected(host: String, port: Int) {}
        fun onTelemetry(samples: List<TelemetrySample>, snapshot: Map<String, Double>) {}
        fun onReply(frame: DecodedFrame) {}
        fun onFrame(frame: DecodedFrame) {}
        fun onError(error: Throwable) {}
        fun onClosed() {}
    }

    companion object {
        const val DEFAULT_HOST = "192.168.88.88"
        const val DEFAULT_PORT = 6666
        const val CONNECT_TIMEOUT_MS = 12400
        const val SOCKET_TIMEOUT_MS = 300000
        const val RCVBUF = 8192
        const val SNDBUF = 5120
        const val TRAFFIC_CLASS = 28
    }

    private val parser = FrameParser({ bytes -> onFrameBytes(bytes) }, resyncOnHeader)
    private var stream = TelemetryAssembler(emptyList(), seq)
    private var socket: Socket? = null
    private val closed = AtomicBoolean(true)
    private val latest = linkedMapOf<String, Double>()
    private var reader: Thread? = null

    val isConnected: Boolean get() = socket?.isConnected == true && !closed.get()

    fun connect() {
        if (isConnected) return
        val s = Socket()
        binder?.bind(s)
        s.tcpNoDelay = true
        s.receiveBufferSize = RCVBUF
        s.sendBufferSize = SNDBUF
        s.keepAlive = false
        s.soTimeout = SOCKET_TIMEOUT_MS
        try {
            s.trafficClass = TRAFFIC_CLASS
        } catch (_: Exception) {
            // Some Wi-Fi drivers reject traffic-class hints; the link still works.
        }
        s.setPerformancePreferences(100, 100, 10)
        s.connect(InetSocketAddress(host, port), connectTimeoutMs)
        socket = s
        closed.set(false)
        parser.reset()
        listener.onConnected(host, port)
        reader = Thread({ readLoop(s) }, "aracer-rx").apply {
            isDaemon = true
            start()
        }
    }

    private fun readLoop(s: Socket) {
        val buffer = ByteArray(4096)
        try {
            while (!closed.get()) {
                val read = s.getInputStream().read(buffer)
                if (read < 0) break
                if (read > 0) parser.feed(buffer.copyOf(read))
            }
        } catch (e: IOException) {
            if (!closed.get()) listener.onError(e)
        } finally {
            closed.set(true)
            listener.onClosed()
        }
    }

    private fun onFrameBytes(bytes: ByteArray) {
        val decoded = try {
            FrameCodec.decode(bytes)
        } catch (e: Exception) {
            listener.onError(e)
            return
        }
        listener.onFrame(decoded)

        if (isTelemetryId(decoded.canId, seq)) {
            val done = stream.push(decoded) ?: return
            for (sample in done) latest[sample.parameter.name] = sample.value
            listener.onTelemetry(done, latest.toMap())
            return
        }
        if (isSdoReplyId(decoded.canId, seq)) listener.onReply(decoded)
    }

    /** Transmit a frame. [obj] is the addressed SDO object, not a CAN ID. */
    fun send(frame: ByteArray, obj: Int, operation: Operation = Operation.READ) {
        assertOperation(obj, operation)
        val s = socket
        if (s == null || closed.get()) throw IllegalStateException("not connected")
        s.getOutputStream().write(frame)
        s.getOutputStream().flush()
    }

    fun requestDeviceName(): ByteArray {
        val frame = deviceNameFrame(seq)
        send(frame, Objects.DEVICE_NAME)
        return frame
    }

    fun requestIdentity(sub: Int = 0): ByteArray {
        val frame = identityFrame(seq, sub)
        send(frame, Objects.IDENTITY)
        return frame
    }

    fun subscribe(params: List<Parameter>): List<ByteArray> {
        stream = TelemetryAssembler(params, seq)
        val frames = subscribeAllFrames(params, seq)
        for (frame in frames) send(frame, Objects.TPDO1_MAP, Operation.SUBSCRIBE)
        return frames
    }

    fun snapshot(): Map<String, Double> = latest.toMap()

    fun close() {
        closed.set(true)
        try {
            socket?.close()
        } catch (_: IOException) {
        }
        socket = null
        reader?.interrupt()
        reader = null
    }
}
