package com.minimotonav.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.time.Instant
import java.time.ZoneOffset

class ReferenceTest {
    private val fixedNow = Instant.parse("2026-06-18T03:37:36.600Z").toEpochMilli()
    private val configText: String by lazy {
        val file = File("src/main/assets/reference-config.txt")
        check(file.exists()) { "missing ${file.absolutePath}" }
        file.readText()
    }

    private val golden =
        "\$GNGGA,033736.600,2306.2437,N,12013.3600,E,1,,,,M,,M,,*59\n" +
            "\$GNRMC,033736.600,A,2306.2437,N,12013.3600,E,17.279,0.00,180626,,,A*74\n" +
            "\$RC3,033736.600,1,,,,,,,11194,100.0,40.00,-28,25.55,91.2,14.1,84,39,0.0,32.758,3.276,0.000,0.000,0.000*21\n"

    private fun frame(hex: String): ByteArray =
        ByteArray(hex.length / 2) { i ->
            ((Character.digit(hex[i * 2], 16) shl 4) or Character.digit(hex[i * 2 + 1], 16)).toByte()
        }

    @Test
    fun `bundled config parses to the reference PDO map`() {
        val map = parsePdoMap(configText)
        assertEquals(38, map.fields.size)
        assertEquals(54, map.cycleBytes)
        assertEquals(6, computeEmitIndex(map.fields))
        assertEquals(PdoField(0, 1), map.fields[111])
        assertEquals(PdoField(3, 2), map.fields[114])
        assertEquals(PdoField(24, 2), map.fields[26])
        assertEquals(PdoField(36, 2), map.fields[45])
        assertNull(map.fields[999])
    }

    @Test
    fun `reference config separates watchdog and init frames`() {
        val frames = parseReferenceConfig(configText)
        assertEquals(1, frames.watchdog.size)
        assertTrue(frames.init.size > 40)
        assertEquals(0x07, frames.watchdog[0][6].toInt() and 0xff)
        assertEquals(0x0b, frames.watchdog[0][7].toInt() and 0xff)
        for (frame in frames.init) assertEquals(19, frame.size)
        assertTrue(frames.init.count { (it[6].toInt() and 0xff) == 0x06 && (it[7].toInt() and 0xff) == 0x02 } >= 40)
    }

    @Test
    fun `sample stream decodes to the reference RC3 GGA RMC output`() {
        val decoder = ReferenceDecoder(ReferenceProfile.parse(configText)) { fixedNow }
        val out = StringBuilder()
        for (hex in REFERENCE_SAMPLE_FRAMES) out.append(decoder.decodeFrame(frame(hex)))
        assertEquals(golden, out.toString())
    }

    @Test
    fun `reference fields match the hand-computed sample values`() {
        val decoder = ReferenceDecoder(ReferenceProfile.parse(configText)) { fixedNow }
        for (hex in REFERENCE_SAMPLE_FRAMES) decoder.decodeFrame(frame(hex))
        assertEquals(3L, decoder.raw(111))
        assertEquals(600L, decoder.raw(114))
        assertEquals(32L, decoder.raw(124))
        assertEquals(11194L, decoder.raw(26))
        assertEquals(100.0, decoder.phy(34), 1e-9)
        assertEquals(39.99609375, decoder.phy(51), 1e-9)
        assertEquals(-28.0, decoder.phy(108), 1e-9)
        assertEquals(25.55, decoder.phy(55), 1e-9)
        assertEquals(91.171875, decoder.phy(45), 1e-9)
        assertEquals(14.1, decoder.phy(29), 1e-9)
        assertEquals(84.0, decoder.phy(33), 1e-9)
        assertEquals(39.0, decoder.phy(24), 1e-9)
    }

    @Test
    fun `NMEA sentences carry independent checksums`() {
        for (line in golden.trim().split("\n")) {
            val star = line.lastIndexOf('*')
            val body = line.substring(1, star)
            var sum = 0
            for (ch in body) sum = sum xor ch.code
            assertEquals(line.substring(star + 1), (sum and 0xff).toString(16).uppercase().padStart(2, '0'))
        }
    }

    @Test
    fun `decoder only emits on its final frame index`() {
        val decoder = ReferenceDecoder(ReferenceProfile.parse(configText)) { fixedNow }
        for (hex in REFERENCE_SAMPLE_FRAMES.take(5)) {
            assertEquals("", decoder.decodeFrame(frame(hex)))
        }
        assertNotEquals("", decoder.decodeFrame(frame(REFERENCE_SAMPLE_FRAMES[5])))
        assertEquals("", decoder.decodeFrame(frame(REFERENCE_SAMPLE_FRAMES[6])))
        assertEquals("", decoder.decodeFrame(frame(REFERENCE_SAMPLE_FRAMES[7])))
    }

    @Test
    fun `frameCanId reads the big-endian CAN field`() {
        assertEquals(0x182, frameCanId(frame(REFERENCE_SAMPLE_FRAMES[0])))
        assertNull(frameCanId(ByteArray(7)))
    }

    @Test
    fun `sample cycle reproduces the six telemetry payloads`() {
        val cycle = ByteArray(64)
        for (hex in REFERENCE_SAMPLE_FRAMES) {
            val f = frame(hex)
            val index = f[10].toInt() and 0xff
            if (index == 0 || index > 16) continue
            val base = (index - 1) * 7
            for (k in 0 until 7) cycle[base + k] = f[11 + k]
        }
        val decoder = ReferenceDecoder(ReferenceProfile.parse(configText)) { fixedNow }
        var out = ""
        for (i in 0 until 6) {
            val payload = byteArrayOf((i + 1).toByte()) + cycle.copyOfRange(i * 7, i * 7 + 7)
            val wire = FrameCodec.encode(ReferenceIds.TELEMETRY, payload)
            out += decoder.decodeFrame(wire)
        }
        assertEquals(golden, out)
    }
}
