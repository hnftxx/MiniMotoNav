package com.minimotonav.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class FrameTest {
    @Test
    fun `encode produces the 19-byte header`() {
        val f = FrameCodec.encode(0x600, byteArrayOf(0x23, 0x08, 0x10, 0x00, 0, 0, 0, 0))
        assertEquals(FrameCodec.FRAME_LEN, f.size)
        assertEquals(FrameCodec.MAGIC, FrameCodec.u8(f[0]))
        assertEquals(FrameCodec.PROTOCOL, FrameCodec.u8(f[1]))
        assertEquals(FrameCodec.FRAME_TYPE, FrameCodec.u8(f[2]))
        assertEquals(FrameCodec.HEADER_LEN, FrameCodec.u8(f[3]))
    }

    @Test
    fun `checksum is 2s complement over bytes 2 to 17`() {
        val f = FrameCodec.encode(0x680, byteArrayOf(0x00))
        assertEquals(FrameCodec.checksum(f), FrameCodec.u8(f[18]))
        assertTrue(FrameCodec.verifyChecksum(f))
        var sum = 0
        for (i in 2..18) sum += FrameCodec.u8(f[i])
        assertEquals(0, sum and 0xff)
    }

    @Test
    fun `extended flag carries a 29-bit CAN ID`() {
        val f = FrameCodec.encode(0x1008, ByteArray(0), FrameCodec.FLAG_EXTENDED)
        assertEquals(FrameCodec.FLAG_EXTENDED, FrameCodec.u8(f[8]) and FrameCodec.FLAG_EXTENDED)
        assertEquals(0x1008, FrameCodec.decode(f).canId)

        val std = FrameCodec.encode(0x600, ByteArray(0))
        assertEquals(0, FrameCodec.u8(std[8]) and FrameCodec.FLAG_EXTENDED)
        assertEquals(0x600, FrameCodec.decode(std).canId)
    }

    @Test
    fun `decoder masks an unflagged extended ID to 11 bits`() {
        val flagged = FrameCodec.encode(0x1008, ByteArray(0), FrameCodec.FLAG_EXTENDED)
        assertEquals(0x1008, FrameCodec.decode(flagged).canId)

        val unflagged = FrameCodec.encode(0x1008, ByteArray(0), 0)
        assertEquals(0x1008 and FrameCodec.CAN_ID_11BIT_MASK, FrameCodec.decode(unflagged).canId)
        assertEquals(8, FrameCodec.decode(unflagged).canId)
    }

    @Test
    fun `big-endian CAN ID field round-trips`() {
        for (canId in listOf(0, 1, 255, 256, 0x600, 0x1008, 0x1a00, 0x1fffffff)) {
            val flags = if (canId > 0x7ff) FrameCodec.FLAG_EXTENDED else 0
            val f = FrameCodec.encode(canId, ByteArray(0), flags)
            assertEquals(canId, FrameCodec.decode(f).rawCanId)
        }
    }

    @Test
    fun `multipart flag round-trips`() {
        val f = FrameCodec.encode(1664, byteArrayOf(1, 2, 3), FrameCodec.FLAG_EXTENDED or FrameCodec.FLAG_MULTIPART)
        val d = FrameCodec.decode(f)
        assertTrue(d.multipart)
        assertTrue(d.extended)
        assertArrayEquals(byteArrayOf(1, 2, 3), d.payload)
    }

    @Test
    fun `payload is bounded to 8 bytes in a single frame`() {
        assertThrows(FrameError::class.java) {
            FrameCodec.encode(1664, ByteArray(9))
        }
        val f = FrameCodec.encode(1664, ByteArray(8) { 0xaa.toByte() })
        assertEquals(8, FrameCodec.decode(f).payload.size)
    }

    @Test
    fun `decoder rejects a corrupted checksum`() {
        val f = FrameCodec.encode(4104, ByteArray(0), FrameCodec.FLAG_EXTENDED)
        f[18] = ((FrameCodec.u8(f[18]) + 1) and 0xff).toByte()
        assertThrows(FrameError::class.java) { FrameCodec.decode(f) }
    }

    @Test
    fun `decoder rejects a bad header`() {
        val f = FrameCodec.encode(1664, byteArrayOf(0))
        f[0] = 0
        assertThrows(FrameError::class.java) { FrameCodec.decode(f) }
    }

    @Test
    fun `parser reassembles a frame split across chunks`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) })
        val wire = FrameCodec.encode(4120, ByteArray(0), FrameCodec.FLAG_EXTENDED)
        for (byte in wire) parser.feed(byteArrayOf(byte))
        assertEquals(1, frames.size)
        assertEquals(4120, FrameCodec.decode(frames[0]).canId)
    }

    @Test
    fun `parser handles several frames in one chunk`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) })
        val a = FrameCodec.encode(1665, byteArrayOf(0))
        val b = FrameCodec.encode(4104, ByteArray(0), FrameCodec.FLAG_EXTENDED)
        val c = FrameCodec.encode(6656, byteArrayOf(1, 0x40, 0x1a, 0x00, 0x10), FrameCodec.FLAG_EXTENDED)
        parser.feed(a + b + c)
        assertEquals(3, frames.size)
        assertEquals(listOf(1665, 4104, 6656), frames.map { FrameCodec.decode(it).canId })
    }

    @Test
    fun `parser accepts payload bytes that equal header bytes`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) })
        val payload = byteArrayOf(
            0xf8.toByte(), 0x01, 0xc0.toByte(), 0xf8.toByte(),
            0x01, 0xc0.toByte(), 0xf8.toByte(), 0x01,
        )
        parser.feed(FrameCodec.encode(1664, payload))
        assertEquals(1, frames.size)
        assertArrayEquals(payload, FrameCodec.decode(frames[0]).payload)
    }

    @Test
    fun `parser resynchronises after garbage`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) })
        parser.feed(byteArrayOf(0x00, 0x11, 0x22, 0x33))
        parser.feed(FrameCodec.encode(1665, byteArrayOf(0)))
        assertEquals(1, frames.size)
        assertTrue(parser.stats.discarded > 0)
    }

    @Test
    fun `parser counts checksum errors without emitting a frame`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) })
        val bad = FrameCodec.encode(1665, byteArrayOf(0))
        bad[18] = ((FrameCodec.u8(bad[18]) + 1) and 0xff).toByte()
        parser.feed(bad)
        assertEquals(0, frames.size)
        assertEquals(1L, parser.stats.checksumErrors)
    }

    @Test
    fun `parser recovers after a bad checksum`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) })
        val bad = FrameCodec.encode(1665, byteArrayOf(0))
        bad[18] = ((FrameCodec.u8(bad[18]) + 1) and 0xff).toByte()
        parser.feed(bad + FrameCodec.encode(4104, ByteArray(0), FrameCodec.FLAG_EXTENDED))
        assertEquals(1, frames.size)
        assertEquals(4104, FrameCodec.decode(frames[0]).canId)
    }

    @Test
    fun `parser loses one frame after a truncation`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) })
        val full = FrameCodec.encode(1664, byteArrayOf(0xaa.toByte()))
        parser.feed(full.copyOfRange(0, 7))
        parser.feed(FrameCodec.encode(1665, byteArrayOf(0)))
        parser.feed(FrameCodec.encode(4120, ByteArray(0), FrameCodec.FLAG_EXTENDED))
        assertEquals(1, frames.size)
        assertEquals(4120, FrameCodec.decode(frames[0]).canId)
    }

    @Test
    fun `resyncOnHeader recovers the frame after a truncation`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) }, resyncOnHeader = true)
        val full = FrameCodec.encode(1664, byteArrayOf(0xaa.toByte()))
        parser.feed(full.copyOfRange(0, 7))
        parser.feed(FrameCodec.encode(1665, byteArrayOf(0)))
        assertEquals(1, frames.size)
        assertEquals(1665, FrameCodec.decode(frames[0]).canId)
        assertTrue(parser.stats.headerResyncs > 0)
    }

    @Test
    fun `resyncOnHeader does not break normal streams`() {
        val frames = mutableListOf<ByteArray>()
        val parser = FrameParser({ frames.add(it) }, resyncOnHeader = true)
        val input = listOf(
            FrameCodec.encode(1665, byteArrayOf(0)),
            FrameCodec.encode(4104, ByteArray(0), FrameCodec.FLAG_EXTENDED),
            FrameCodec.encode(1664 or 26, byteArrayOf(0x81.toByte(), 0x10, 0x27)),
        )
        for (f in input) parser.feed(f)
        assertEquals(3, frames.size)
        assertEquals(0L, parser.stats.headerResyncs)
    }
}
