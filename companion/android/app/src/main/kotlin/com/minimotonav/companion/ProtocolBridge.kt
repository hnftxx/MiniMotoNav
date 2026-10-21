package com.minimotonav.companion

import android.content.Context
import android.os.Handler
import android.os.Looper
import com.minimotonav.protocol.DecodedFrame
import com.minimotonav.protocol.Operation
import com.minimotonav.protocol.Parameters
import com.minimotonav.protocol.TelemetrySample
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import java.util.concurrent.Executors

/**
 * Flutter <-> Kotlin bridge. All ECU traffic is TCP and read-only; the Dart
 * side cannot select an operation class.
 */
class ProtocolBridge(context: Context? = null) :
    MethodChannel.MethodCallHandler,
    EventChannel.StreamHandler {
    companion object {
        const val METHOD_CHANNEL = "moto/protocol"
        const val EVENT_CHANNEL = "moto/protocol/events"
    }

    private val main = Handler(Looper.getMainLooper())
    private val worker = Executors.newSingleThreadExecutor { r -> Thread(r, "aracer-io").apply { isDaemon = true } }
    private val binder = context?.let { WifiSocketBinder(it) { state -> emitStatus(state) } }

    @Volatile
    private var events: EventChannel.EventSink? = null

    @Volatile
    private var client: AracerClient? = null

    private fun emit(value: Map<String, Any?>) {
        main.post { events?.success(value) }
    }

    private fun emitStatus(state: String, message: String? = null) {
        emit(mapOf("type" to "status", "state" to state, "message" to message))
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "connect" -> {
                val host = call.argument<String>("host") ?: AracerClient.DEFAULT_HOST
                val port = call.argument<Int>("port") ?: AracerClient.DEFAULT_PORT
                val ssid = call.argument<String>("ssid")
                val password = call.argument<String>("password")
                connect(host, port, ssid, password, result)
            }
            "disconnect" -> {
                client?.close()
                client = null
                binder?.release()
                emitStatus("closed")
                result.success(true)
            }
            "subscribe" -> {
                @Suppress("UNCHECKED_CAST")
                val names = call.argument<List<String>>("names") ?: emptyList()
                val params = names.mapNotNull { Parameters.byName(it) }
                val active = client
                if (active == null) {
                    result.error("not_connected", "connect first", null)
                    return
                }
                try {
                    active.subscribe(params)
                    result.success(params.size)
                } catch (e: Exception) {
                    result.error("subscribe_failed", e.message, null)
                }
            }
            "requestDeviceName" -> {
                try {
                    client?.requestDeviceName()
                    result.success(true)
                } catch (e: Exception) {
                    result.error("not_connected", e.message, null)
                }
            }
            "requestIdentity" -> {
                try {
                    client?.requestIdentity(call.argument<Int>("sub") ?: 0)
                    result.success(true)
                } catch (e: Exception) {
                    result.error("not_connected", e.message, null)
                }
            }
            "snapshot" -> result.success(client?.snapshot() ?: emptyMap<String, Double>())
            else -> result.notImplemented()
        }
    }

    private fun connect(
        host: String,
        port: Int,
        ssid: String?,
        password: String?,
        result: MethodChannel.Result,
    ) {
        client?.close()
        val next = AracerClient(
            host = host,
            port = port,
            binder = binder,
            listener = object : AracerClient.Listener {
                override fun onConnected(host: String, port: Int) {
                    emitStatus("connected", "$host:$port")
                }

                override fun onTelemetry(samples: List<TelemetrySample>, snapshot: Map<String, Double>) {
                    emit(
                        mapOf(
                            "type" to "telemetry",
                            "values" to snapshot,
                            "samples" to samples.map {
                                mapOf(
                                    "name" to it.parameter.name,
                                    "absid" to it.absid,
                                    "raw" to it.raw,
                                    "value" to it.value,
                                    "unit" to it.parameter.unit,
                                )
                            },
                        ),
                    )
                }

                override fun onReply(frame: DecodedFrame) {
                    emit(
                        mapOf(
                            "type" to "reply",
                            "canId" to frame.canId,
                            "payload" to frame.payload.map { it.toInt() and 0xff },
                        ),
                    )
                }

                override fun onError(error: Throwable) {
                    emitStatus("error", error.message ?: error.javaClass.simpleName)
                }

                override fun onClosed() {
                    emitStatus("closed")
                }
            },
        )
        client = next
        emitStatus("connecting", "$host:$port")
        worker.execute {
            try {
                binder?.prepare(ssid, password)
                next.connect()
                main.post { result.success(true) }
            } catch (e: Exception) {
                main.post { result.error("connect_failed", e.message, null) }
            }
        }
    }

    override fun onListen(arguments: Any?, sink: EventChannel.EventSink) {
        events = sink
    }

    override fun onCancel(arguments: Any?) {
        events = null
    }

    fun dispose() {
        client?.close()
        client = null
        binder?.release()
        worker.shutdownNow()
    }
}
