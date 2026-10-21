package com.minimotonav.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ParametersTest {
    @Test
    fun `dashboard parameters exist and are the required six`() {
        assertEquals(
            listOf("RPM", "TPS_Percent", "T_Eng", "Volt_Batt", "SA", "AFR_Target"),
            Parameters.DASHBOARD,
        )
        for (name in Parameters.DASHBOARD) assertNotNull("missing $name", Parameters.byName(name))
    }

    @Test
    fun `parameter table has unique absids and names`() {
        assertEquals(Parameters.ALL.size, Parameters.ALL.map { it.absid }.toSet().size)
        assertEquals(Parameters.ALL.size, Parameters.ALL.map { it.name }.toSet().size)
    }

    @Test
    fun `RPM decodes 1 to 1`() {
        val p = Parameters.byName("RPM")!!
        assertEquals(26, p.absid)
        assertEquals(Parameters.WIDTH_U16, p.width)
        for (raw in listOf(0, 1000, 5000, 12000, 20000)) {
            assertEquals(raw.toDouble(), Parameters.decodeValue(p, raw), 1e-9)
        }
    }

    @Test
    fun `TPS_Percent applies divisor 2_55`() {
        val p = Parameters.byName("TPS_Percent")!!
        assertEquals(34, p.absid)
        assertEquals(Parameters.WIDTH_BYTE, p.width)
        assertEquals(0.0, Parameters.decodeValue(p, 0), 1e-9)
        assertEquals(50.19607843, Parameters.decodeValue(p, 128), 1e-6)
        assertEquals(100.0, Parameters.decodeValue(p, 255), 1e-9)
    }

    @Test
    fun `T_Eng applies offset 28`() {
        val p = Parameters.byName("T_Eng")!!
        assertEquals(33, p.absid)
        assertEquals(-28.0, Parameters.decodeValue(p, 0), 1e-9)
        assertEquals(0.0, Parameters.decodeValue(p, 28), 1e-9)
        assertEquals(72.0, Parameters.decodeValue(p, 100), 1e-9)
        assertEquals(227.0, Parameters.decodeValue(p, 255), 1e-9)
    }

    @Test
    fun `SA applies divisor 4 and offset 1024`() {
        val p = Parameters.byName("SA")!!
        assertEquals(25, p.absid)
        assertEquals(Parameters.WIDTH_U16, p.width)
        assertEquals(0.0, Parameters.decodeValue(p, 1024), 1e-9)
        assertEquals(127.0, Parameters.decodeValue(p, 2048), 1e-9)
        assertEquals(-180.0, Parameters.decodeValue(p, 0), 1e-9)
    }

    @Test
    fun `Volt_Batt and AFR_Target apply divisor 10`() {
        val volt = Parameters.byName("Volt_Batt")!!
        assertEquals(12.0, Parameters.decodeValue(volt, 120), 1e-9)
        assertEquals(25.5, Parameters.decodeValue(volt, 255), 1e-9)
        val afr = Parameters.byName("AFR_Target")!!
        assertEquals(13.2, Parameters.decodeValue(afr, 132), 1e-9)
    }

    @Test
    fun `encode is the inverse of decode at representable points`() {
        for (p in Parameters.ALL) {
            val rawSpan = when (p.width) {
                Parameters.WIDTH_BYTE -> 0xffL
                Parameters.WIDTH_U16 -> 0xffffL
                else -> 0xffffffffL
            }
            val step = Math.abs(1.0 / p.divisor)
            for (i in 0..4) {
                val v = Parameters.clamp(p.min + (p.max - p.min) * (i / 4.0), p.min, p.max)
                val raw = Parameters.encodeValue(p, v)
                if (raw >= rawSpan) continue
                val back = Parameters.decodeValue(p, raw)
                assertTrue(
                    "${p.name}: $v -> $raw -> $back (step $step)",
                    Math.abs(back - v) <= step / 2 + 1e-6,
                )
            }
        }
    }

    @Test
    fun `encode respects width ceilings`() {
        assertEquals(255L, Parameters.encodeValue(Parameters.byName("TPS_Percent")!!, 1000.0))
        assertEquals(20000L, Parameters.encodeValue(Parameters.byName("RPM")!!, 1e9))
        assertEquals(2147483647L, Parameters.encodeValue(Parameters.byName("Total_Fuel")!!, 1e12))
    }

    @Test
    fun `uint32 read and write survives values above int32`() {
        val p = Parameters.byName("Total_Fuel")!!
        assertEquals(Parameters.WIDTH_U32, p.width)
        val buf = ByteArray(4)
        Parameters.writeLE(buf, 0, Parameters.WIDTH_U32, 2147483647L)
        assertArrayEquals(byteArrayOf(0xff.toByte(), 0xff.toByte(), 0xff.toByte(), 0x7f), buf)
        assertEquals(2147483647L, Parameters.readLE(buf, 0, Parameters.WIDTH_U32))
        Parameters.writeLE(buf, 0, Parameters.WIDTH_U32, 4294967295L)
        assertEquals(4294967295L, Parameters.readLE(buf, 0, Parameters.WIDTH_U32))
    }

    @Test
    fun `signed divisor parameters invert correctly`() {
        val p = Parameters.byName("Inj_Angle")!!
        assertEquals(Parameters.WIDTH_BYTE, p.width)
        assertEquals(0.0, Parameters.decodeValue(p, 105), 1e-9)
        assertEquals(200.0, Parameters.decodeValue(p, 55), 1e-9)
        assertEquals(420.0, Parameters.decodeValue(p, 0), 1e-9)
    }

    @Test
    fun `readLE and writeLE are little-endian`() {
        val buf = byteArrayOf(0x34, 0x12, 0x00, 0x00)
        assertEquals(0x1234L, Parameters.readLE(buf, 0, Parameters.WIDTH_U16))

        val out = ByteArray(4)
        Parameters.writeLE(out, 0, Parameters.WIDTH_U16, 0x1234)
        assertArrayEquals(byteArrayOf(0x34, 0x12, 0, 0), out)

        Parameters.writeLE(out, 0, Parameters.WIDTH_U32, 0xdeadbeefL)
        assertArrayEquals(byteArrayOf(0xef.toByte(), 0xbe.toByte(), 0xad.toByte(), 0xde.toByte()), out)
        assertEquals(0xdeadbeefL, Parameters.readLE(out, 0, Parameters.WIDTH_U32))
    }

    @Test
    fun `telemetry is big-endian RPM raw 5000 is 13 88`() {
        val buf = ByteArray(2)
        Parameters.writeBE(buf, 0, Parameters.WIDTH_U16, 5000)
        assertArrayEquals(byteArrayOf(0x13, 0x88.toByte()), buf)
        assertEquals(5000L, Parameters.readBE(buf, 0, Parameters.WIDTH_U16))
    }

    @Test
    fun `selector encodes width and absid`() {
        val cases = listOf(
            Triple(2, 26, 0x401a0010L),
            Triple(1, 34, 0x40220008L),
            Triple(1, 33, 0x40210008L),
            Triple(1, 29, 0x401d0008L),
            Triple(2, 25, 0x40190010L),
        )
        for ((width, absid, selector) in cases) {
            assertEquals(selector, buildSelector(width, absid))
            assertEquals(Selector(width, absid), parseSelector(selector))
        }
    }

    @Test
    fun `selector round-trips for every parameter`() {
        for (p in Parameters.ALL) {
            val back = parseSelector(buildSelector(p.width, p.absid))
            assertEquals(p.width, back.width)
            assertEquals(p.absid, back.absid)
        }
    }

    @Test
    fun `parameterByAbsid returns null for unknown ids`() {
        assertNull(Parameters.byAbsid(9999))
        assertNull(Parameters.byName("NOPE"))
    }
}
