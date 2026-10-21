package com.minimotonav.moto_companion

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification

/**
 * Forwards Google Maps navigation notifications to the Flutter parser.
 *
 * Nothing is parsed or acted on here; only the raw text fields, package, post
 * time and key leave the system service. The listener never grants itself any
 * other notification access and ignores every other package.
 */
class MapsNotificationListener : NotificationListenerService() {
    override fun onListenerConnected() {
        NotificationRelay.post(mapOf("event" to "connected"))
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        if (sbn.packageName != MAPS_PACKAGE) return
        val notification = sbn.notification ?: return
        if (notification.flags and Notification.FLAG_GROUP_SUMMARY != 0) return
        val extras = notification.extras ?: return
        NotificationRelay.post(
            mapOf(
                "event" to "posted",
                "notification" to mapOf(
                    "key" to sbn.key,
                    "package" to sbn.packageName,
                    "postTime" to sbn.postTime,
                    "title" to extras.getCharSequence(Notification.EXTRA_TITLE)?.toString(),
                    "text" to extras.getCharSequence(Notification.EXTRA_TEXT)?.toString(),
                    "bigText" to extras.getCharSequence(Notification.EXTRA_BIG_TEXT)?.toString(),
                    "subText" to extras.getCharSequence(Notification.EXTRA_SUB_TEXT)?.toString(),
                ),
            ),
        )
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification) {
        if (sbn.packageName != MAPS_PACKAGE) return
        NotificationRelay.post(
            mapOf(
                "event" to "removed",
                "key" to sbn.key,
                "package" to sbn.packageName,
            ),
        )
    }

    companion object {
        private const val MAPS_PACKAGE = "com.google.android.apps.maps"
    }
}
