package com.minimotonav.moto_companion

import android.content.Intent
import android.provider.Settings
import com.minimotonav.companion.BleBridge
import com.minimotonav.companion.ProtocolBridge
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private var protocol: ProtocolBridge? = null
    private var ble: BleBridge? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        val messenger = flutterEngine.dartExecutor.binaryMessenger

        val protocolBridge = ProtocolBridge(this)
        protocol = protocolBridge
        MethodChannel(messenger, ProtocolBridge.METHOD_CHANNEL)
            .setMethodCallHandler(protocolBridge)
        EventChannel(messenger, ProtocolBridge.EVENT_CHANNEL)
            .setStreamHandler(protocolBridge)

        val bleBridge = BleBridge(this)
        ble = bleBridge
        MethodChannel(messenger, BleBridge.METHOD_CHANNEL)
            .setMethodCallHandler(bleBridge)
        EventChannel(messenger, BleBridge.EVENT_CHANNEL)
            .setStreamHandler(bleBridge)

        EventChannel(messenger, MAPS_EVENT_CHANNEL).setStreamHandler(
            object : EventChannel.StreamHandler {
                override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                    NotificationRelay.attach(events)
                }

                override fun onCancel(arguments: Any?) {
                    NotificationRelay.detach()
                }
            },
        )
        MethodChannel(messenger, MAPS_CONTROL_CHANNEL).setMethodCallHandler { call, result ->
            when (call.method) {
                "status" -> result.success(
                    mapOf("enabled" to isNotificationListenerEnabled()),
                )
                "openSettings" -> {
                    val intent = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    startActivity(intent)
                    result.success(null)
                }
                else -> result.notImplemented()
            }
        }
    }

    private fun isNotificationListenerEnabled(): Boolean {
        val enabled = Settings.Secure.getString(
            contentResolver,
            "enabled_notification_listeners",
        ) ?: return false
        return enabled.split(':').any {
            it.startsWith("$packageName/")
        }
    }

    override fun onDestroy() {
        ble?.dispose()
        ble = null
        protocol?.dispose()
        protocol = null
        NotificationRelay.detach()
        super.onDestroy()
    }

    companion object {
        private const val MAPS_EVENT_CHANNEL = "moto/maps"
        private const val MAPS_CONTROL_CHANNEL = "moto/maps/control"
    }
}