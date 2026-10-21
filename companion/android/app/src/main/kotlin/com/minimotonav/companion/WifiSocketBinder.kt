package com.minimotonav.companion

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import java.net.Socket
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * Binds the ECU socket to the ECU Wi-Fi network while Android keeps the default
 * route on cellular for navigation traffic.
 *
 * Android 10+ joins the ECU SSID with a WifiNetworkSpecifier, which acquires the
 * network for this process only. The app never calls bindProcessToNetwork and
 * never makes the ECU Wi-Fi the default network.
 */
class WifiSocketBinder(
    context: Context,
    private val onStatus: (String) -> Unit = {},
) : SocketBinder {
    private val manager =
        context.getSystemService(ConnectivityManager::class.java)

    @Volatile
    private var network: Network? = null
    private var callback: ConnectivityManager.NetworkCallback? = null

    /**
     * Requests the ECU network. Blocks the caller (the protocol worker thread)
     * until the network is available, unavailable, or the request times out.
     */
    fun prepare(ssid: String?, passphrase: String?) {
        val name = ssid?.trim()?.trim('"')?.takeIf { it.isNotEmpty() } ?: return
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            onStatus("legacy")
            return
        }
        val connectivity = manager ?: run {
            onStatus("unavailable")
            return
        }
        release()

        val specifier = WifiNetworkSpecifier.Builder()
            .setSsid(name)
            .apply {
                if (!passphrase.isNullOrEmpty()) setWpa2Passphrase(passphrase)
            }
            .build()

        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .addCapability(NetworkCapabilities.NET_CAPABILITY_TRUSTED)
            .addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_CONGESTED)
            .removeCapability(20) // NET_CAPABILITY_NOT_BANDWIDTH_CONSTRAINED
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_SUSPENDED)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_ROAMING)
            .setNetworkSpecifier(specifier)
            .build()

        val latch = CountDownLatch(1)
        val pending = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(available: Network) {
                network = available
                onStatus("connecting")
                latch.countDown()
            }

            override fun onUnavailable() {
                onStatus("unavailable")
                latch.countDown()
            }

            override fun onLost(lost: Network) {
                if (network == lost) network = null
                onStatus("disconnected")
            }
        }
        callback = pending
        onStatus("requesting")
        try {
            connectivity.requestNetwork(request, pending, NETWORK_TIMEOUT_MS)
        } catch (_: SecurityException) {
            onStatus("permission_denied")
            latch.countDown()
        } catch (_: Exception) {
            onStatus("unavailable")
            latch.countDown()
        }
        try {
            latch.await(NETWORK_TIMEOUT_MS.toLong(), TimeUnit.MILLISECONDS)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
        }
        if (network == null && callback != null) onStatus("unavailable")
    }

    override fun bind(socket: Socket) {
        val acquired = network
        if (acquired != null) {
            acquired.bindSocket(socket)
            return
        }
        // Android 9 and older have no per-network Wi-Fi specifier. Bind to a
        // Wi-Fi network that is already joined without changing the app default.
        val connectivity = manager ?: return
        val wifi = connectivity.allNetworks.firstOrNull { candidate ->
            connectivity.getNetworkCapabilities(candidate)
                ?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        } ?: return
        try {
            wifi.bindSocket(socket)
        } catch (_: Exception) {
            // The ECU link reports the connection failure upstream. Navigation
            // stays on the default network.
        }
    }

    fun release() {
        val pending = callback ?: return
        callback = null
        network = null
        try {
            manager?.unregisterNetworkCallback(pending)
        } catch (_: Exception) {
        }
    }

    companion object {
        private const val NETWORK_TIMEOUT_MS = 20000
    }
}
