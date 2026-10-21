package com.minimotonav.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class MessagesTest {
    private fun hex(value: String): ByteArray =
        ByteArray(value.length / 2) { i ->
            ((Character.digit(value[i * 2], 16) shl 4) or Character.digit(value[i * 2 + 1], 16)).toByte()
        }

    private fun bytes(vararg values: Int): ByteArray = ByteArray(values.size) { values[it].toByte() }

    @Test
    fun `short read commands encode 4 3 2 1 bytes`() {
        assertEquals(0x23, FrameCodec.u8(sdoReadData(0x1008, 0, 4)[0]))
        assertEquals(0x27, FrameCodec.u8(sdoReadData(0x1008, 0, 3)[0]))
        assertEquals(0x2b, FrameCodec.u8(sdoReadData(0x1008, 0, 2)[0]))
        assertEquals(0x2f, FrameCodec.u8(sdoReadData(0x1008, 0, 1)[0]))
        assertArrayEquals(bytes(0x08, 0x10, 0x02), sdoReadData(0x1008, 2, 4).copyOfRange(1, 4))
    }

    @Test
    fun `long read carries object sub and LE32 length`() {
        assertArrayEquals(
            bytes(0xc6, 0x08, 0x10, 0x00, 0x0a, 0, 0, 0),
            sdoLongReadData(0x1008, 0, 10),
        )
    }

    @Test
    fun `short write commands encode 4 3 2 1 bytes`() {
        assertEquals(0x4b, FrameCodec.u8(sdoWriteData(0x1800, 5, 64, 2)[0]))
        assertArrayEquals(bytes(0x40, 0x00), sdoWriteData(0x1800, 5, 64, 2).copyOfRange(4, 6))
        assertArrayEquals(bytes(0x06), sdoWriteData(0x1a00, 0, 6, 1).copyOfRange(4, 5))
    }

    @Test
    fun `device name frame is read-only and uses CAN ID 0x600`() {
        val frame = deviceNameFrame(0)
        assertEquals("f801c00e000006000008c60810000a0000003c", frame.toHex())
        val decoded = FrameCodec.decode(frame)
        assertEquals(0x600, decoded.canId)
        assertArrayEquals(bytes(0xc6, 0x08, 0x10, 0x00, 0x0a, 0, 0, 0), decoded.payload)
    }

    @Test
    fun `write objects are rejected outside the allowlist`() {
        val error = assertThrows(ReadOnlyViolation::class.java) {
            readObjectFrame(0, 0x2001, 0, 4)
        }
        assertTrue(error.message!!.contains("read-only"))
    }

    @Test
    fun `subscribe frame maps a selector into TPDO1`() {
        val frame = subscribeFrame(0, 1, 2, 26)
        val decoded = FrameCodec.decode(frame)
        assertEquals(0x600, decoded.canId)
        assertArrayEquals(bytes(0x43, 0x00, 0x1a, 0x01, 0x10, 0x00, 0x1a, 0x40), decoded.payload)
    }

    @Test
    fun `commit frame carries the count`() {
        val frame = subscribeCloseFrame(0, 6)
        assertArrayEquals(bytes(0x4f, 0x00, 0x1a, 0x00, 0x06, 0, 0, 0), FrameCodec.decode(frame).payload)
    }

    @Test
    fun `stream timer is ms little-endian on 0x1800 sub 5`() {
        val frame = setStreamTimerFrame(0, 64)
        assertArrayEquals(bytes(0x4b, 0x00, 0x18, 0x05, 0x40, 0x00, 0, 0), FrameCodec.decode(frame).payload)
    }

    @Test
    fun `subscribeAll emits one frame per parameter plus commit`() {
        val params = listOf("RPM", "TPS_Percent").map { Parameters.byName(it)!! }
        val frames = subscribeAllFrames(params, 3)
        assertEquals(3, frames.size)
        assertEquals(0x603, FrameCodec.decode(frames[0]).canId)
        assertEquals(0x4f, FrameCodec.u8(FrameCodec.decode(frames[2]).payload[0]))
    }

    @Test
    fun `telemetry assembler decodes one block in subscription order`() {
        val params = listOf("RPM", "TPS_Percent", "Volt_Batt").map { Parameters.byName(it)!! }
        val assembler = TelemetryAssembler(params, 2)
        val frame = FrameCodec.decode(
            FrameCodec.encode(0x182, byteArrayOf(0x81.toByte(), 0x13, 0x88.toByte(), 0x80.toByte(), 0xe1.toByte())),
        )
        val out = assembler.push(frame)!!
        assertEquals(3, out.size)
        assertEquals(5000.0, out[0].value, 1e-9)
        assertEquals(0x80 / 2.55, out[1].value, 1e-9)
        assertEquals(22.5, out[2].value, 1e-9)
    }

    @Test
    fun `telemetry assembler joins blocks until the last marker`() {
        val params = listOf("Total_Fuel", "RPM", "SA").map { Parameters.byName(it)!! }
        val assembler = TelemetryAssembler(params, 0)
        assertNull(
            assembler.push(
                FrameCodec.decode(
                    FrameCodec.encode(
                        0x180,
                        byteArrayOf(0x01, 0x00, 0x00, 0x00, 0x01, 0x13, 0x88.toByte(), 0x00),
                    ),
                ),
            ),
        )
        val out = assembler.push(
            FrameCodec.decode(FrameCodec.encode(0x180, byteArrayOf(0x82.toByte(), 0x04, 0x00, 0x01))),
        )!!
        assertEquals(3, out.size)
        assertEquals(1L, out[0].raw)
        assertEquals(5000.0, out[1].value, 1e-9)
        assertEquals(-180.0, out[2].value, 1e-9)
    }

    @Test
    fun `telemetry assembler ignores other sequences`() {
        val assembler = TelemetryAssembler(listOf(Parameters.byName("RPM")!!), 2)
        assertNull(
            assembler.push(
                FrameCodec.decode(FrameCodec.encode(0x183, byteArrayOf(0x81.toByte(), 0x13, 0x88.toByte()))),
            ),
        )
    }

    @Test
    fun `decodeSdoReply reads LE values and acks`() {
        val ack = decodeSdoReply(bytes(0x60, 0x08, 0x10, 0x00, 0, 0, 0, 0))
        assertEquals(0x60, ack.command)
        assertEquals(0x1008, ack.obj)
        assertEquals(0, ack.sub)
        assertTrue(ack.ack)

        val reply = decodeSdoReply(bytes(0x68, 0x1a, 0x40, 0x01, 0x34, 0x12))
        assertEquals(0x1234L, reply.value)
        assertEquals(2, reply.length)
    }

    @Test
    fun `subscribe rejects slot zero`() {
        assertThrows(IllegalArgumentException::class.java) { subscribeFrame(0, 0, 2, 26) }
    }

    @Test
    fun `objects table exposes the observed identifiers`() {
        assertEquals(0x1008, Objects.DEVICE_NAME)
        assertEquals(0x1018, Objects.IDENTITY)
        assertEquals(0x1800, Objects.TPDO1_COMM)
        assertEquals(0x1a00, Objects.TPDO1_MAP)
    }
}

fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it.toInt() and 0xff) }
