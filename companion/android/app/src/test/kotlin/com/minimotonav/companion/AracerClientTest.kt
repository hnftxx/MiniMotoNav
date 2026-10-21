package com.minimotonav.companion

import com.minimotonav.protocol.FrameCodec
import com.minimotonav.protocol.Objects
import com.minimotonav.protocol.Operation
import com.minimotonav.protocol.Parameters
import com.minimotonav.protocol.ReadOnlyViolation
import com.minimotonav.protocol.TelemetrySample
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.InputStream
import java.net.ServerSocket
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

class AracerClientTest {
    @Test
    fun `client subscribes and decodes telemetry from a loopback ECU`() {
        val server = ServerSocket(0)
        val received = AtomicReference<List<TelemetrySample>>()
        val ready = CountDownLatch(1)

        val serverThread = Thread {
            val socket = server.accept()
            val input: InputStream = socket.getInputStream()
            val buffer = ByteArray(19)
            while (true) {
                var read = 0
                while (read < 19) {
                    val n = input.read(buffer, read, 19 - read)
                    if (n < 0) return@Thread
                    read += n
                }
                val frame = FrameCodec.decode(buffer)
                val payload = frame.payload
                if (frame.canId and 0x7f == 0 && payload.isNotEmpty() &&
                    (payload[0].toInt() and 0xff) == 0x4f
                ) {
                    val wire = FrameCodec.encode(
                        0x180,
                        byteArrayOf(0x81.toByte(), 0x13, 0x88.toByte(), 0x80.toByte(), 0xe1.toByte()),
                    )
                    socket.getOutputStream().write(wire)
                    socket.getOutputStream().flush()
                    break
                }
            }
            socket.close()
        }.apply {
            isDaemon = true
            start()
        }

        val client = AracerClient(
            host = "127.0.0.1",
            port = server.localPort,
            connectTimeoutMs = 2000,
            listener = object : AracerClient.Listener {
                override fun onConnected(host: String, port: Int) {
                    println("connected $host:$port")
                }

                override fun onTelemetry(samples: List<TelemetrySample>, snapshot: Map<String, Double>) {
                    received.set(samples)
                    ready.countDown()
                }

                override fun onError(error: Throwable) {
                    error.printStackTrace()
                }
            },
        )

        try {
            client.connect()
            val params = listOf("RPM", "TPS_Percent", "Volt_Batt").map { Parameters.byName(it)!! }
            client.subscribe(params)
            assertTrue("no telemetry", ready.await(3, TimeUnit.SECONDS))
            val samples = received.get()
            assertNotNull(samples)
            assertEquals(3, samples.size)
            assertEquals(5000.0, samples[0].value, 1e-9)
            assertEquals(0x80 / 2.55, samples[1].value, 1e-9)
            assertEquals(22.5, samples[2].value, 1e-9)
            assertEquals(5000.0, client.snapshot()["RPM"]!!, 1e-9)
        } finally {
            client.close()
            server.close()
            serverThread.join(1000)
        }
    }

    @Test
    fun `transport refuses persistent and unclassified objects`() {
        val client = AracerClient(listener = object : AracerClient.Listener {})
        assertThrows(ReadOnlyViolation::class.java) {
            client.send(FrameCodec.encode(0x600, byteArrayOf(0x43, 0x00, 0x1a, 0x00)), 0x2001, Operation.READ)
        }
        assertThrows(ReadOnlyViolation::class.java) {
            client.send(
                FrameCodec.encode(0x600, byteArrayOf(0x43, 0x00, 0x1a, 0x00)),
                Objects.TPDO1_MAP,
                Operation.WRITE,
            )
        }
    }
}
