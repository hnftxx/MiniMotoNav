package com.minimotonav.moto_companion

import android.os.Handler
import android.os.Looper
import io.flutter.plugin.common.EventChannel

/**
 * Bridges the notification listener (a separate Android service) to the
 * Flutter activity's event channel. The newest posted notification is replayed
 * when Flutter attaches, so a listener event that arrives before the UI is
 * ready is not lost.
 */
object NotificationRelay {
    private val main = Handler(Looper.getMainLooper())
    private var sink: EventChannel.EventSink? = null
    private var latest: Map<String, Any?>? = null

    fun attach(next: EventChannel.EventSink?) {
        sink = next
        val replay = latest
        if (next != null && replay != null) {
            main.post { next.success(replay) }
        }
    }

    fun detach() {
        sink = null
    }

    fun post(event: Map<String, Any?>) {
        if (event["event"] == "posted") latest = event
        main.post { sink?.success(event) }
    }
}
