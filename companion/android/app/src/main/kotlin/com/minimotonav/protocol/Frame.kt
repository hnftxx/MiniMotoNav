package com.minimotonav.protocol

/**
 * aRacer X-Series 19-byte frame codec.
 *
 * 0 MAGIC, 1 PROTOCOL, 2 TYPE, 3 HEADER_LEN, 4..7 CAN ID (big-endian),
 * 8 FLAGS, 9 PAYLOAD_LEN, 10..17 payload, 18 checksum =
 * 256 - (sum(bytes[2..17]) and 0xFF).
 */
class FrameError(message: String, val details: Map<String, Int> = emptyMap()) : Exception(message)

data class DecodedFrame(
    val canId: Int,
    val rawCanId: Int,
    val extended: Boolean,
    val multipart: Boolean,
    val flags: Int,
    val payload: ByteArray,
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is DecodedFrame) return false
        return canId == other.canId && rawCanId == other.rawCanId &&
            extended == other.extended && multipart == other.multipart &&
            flags == other.flags && payload.contentEquals(other.payload)
    }

    override fun hashCode(): Int {
        var result = canId
        result = 31 * result + rawCanId
        result = 31 * result + extended.hashCode()
        result = 31 * result + multipart.hashCode()
        result = 31 * result + flags
        result = 31 * result + payload.contentHashCode()
        return result
    }
}

data class ParserStats(
    var bytes: Long = 0,
    var frames: Long = 0,
    var checksumErrors: Long = 0,
    var discarded: Long = 0,
    var midFrameResets: Long = 0,
    var headerResyncs: Long = 0,
)

object FrameCodec {
    const val MAGIC = 0xf8
    const val PROTOCOL = 0x01
    const val FRAME_TYPE = 0xc0
    const val HEADER_LEN = 0x0e
    const val FRAME_LEN = 19
    const val MAX_PAYLOAD = 8

    const val FLAG_EXTENDED = 0x01
    const val FLAG_MULTIPART = 0x02

    const val CAN_ID_11BIT_MAX = 0x7ff
    const val CAN_ID_11BIT_MASK = 0x7ff
    const val CAN_ID_29BIT_MASK = 0x1fffffff

    fun u8(value: Byte): Int = value.toInt() and 0xff

    /** 2's-complement checksum over bytes [2..17]. */
    fun checksum(frame: ByteArray): Int {
        var sum = 0
        for (i in 2..17) sum += u8(frame[i])
        return (256 - (sum and 0xff)) and 0xff
    }

    /** True when sum(bytes[2..18]) == 0 mod 256. */
    fun verifyChecksum(frame: ByteArray): Boolean {
        var sum = 0
        for (i in 2..18) sum += u8(frame[i])
        return (sum and 0xff) == 0
    }

    /**
     * Encodes a frame. [flags] defaults to FLAG_EXTENDED above 0x7FF; the
     * receiver masks an unflagged ID to 11 bits.
     */
    fun encode(canId: Int, payload: ByteArray = ByteArray(0), flags: Int? = null): ByteArray {
        if (canId < 0) throw FrameError("CAN ID out of range: $canId", mapOf("canId" to canId))
        if (payload.size > MAX_PAYLOAD) {
            throw FrameError(
                "payload of ${payload.size} exceeds single-frame limit $MAX_PAYLOAD; use multipart",
                mapOf("length" to payload.size),
            )
        }
        val frame = ByteArray(FRAME_LEN)
        frame[0] = MAGIC.toByte()
        frame[1] = PROTOCOL.toByte()
        frame[2] = FRAME_TYPE.toByte()
        frame[3] = HEADER_LEN.toByte()
        frame[4] = ((canId ushr 24) and 0xff).toByte()
        frame[5] = ((canId ushr 16) and 0xff).toByte()
        frame[6] = ((canId ushr 8) and 0xff).toByte()
        frame[7] = (canId and 0xff).toByte()
        frame[8] = (flags ?: if (canId > CAN_ID_11BIT_MAX) FLAG_EXTENDED else 0).toByte()
        frame[9] = payload.size.toByte()
        payload.copyInto(frame, 10)
        frame[18] = checksum(frame).toByte()
        return frame
    }

    fun decode(frame: ByteArray): DecodedFrame {
        if (frame.size < FRAME_LEN) throw FrameError("frame shorter than $FRAME_LEN bytes")
        if (u8(frame[0]) != MAGIC || u8(frame[1]) != PROTOCOL || u8(frame[2]) != FRAME_TYPE) {
            throw FrameError("bad frame header")
        }
        if (u8(frame[3]) != HEADER_LEN) throw FrameError("unexpected header length ${u8(frame[3])}")
        if (!verifyChecksum(frame)) throw FrameError("checksum mismatch")
        val payloadLen = u8(frame[9])
        if (payloadLen > MAX_PAYLOAD) throw FrameError("payload length $payloadLen exceeds $MAX_PAYLOAD")
        val flags = u8(frame[8])
        val rawCanId = (u8(frame[4]) shl 24) or (u8(frame[5]) shl 16) or
            (u8(frame[6]) shl 8) or u8(frame[7])
        val extended = (flags and FLAG_EXTENDED) != 0
        return DecodedFrame(
            canId = if (extended) rawCanId and CAN_ID_29BIT_MASK else rawCanId and CAN_ID_11BIT_MASK,
            rawCanId = rawCanId,
            extended = extended,
            multipart = (flags and FLAG_MULTIPART) != 0,
            flags = flags,
            payload = frame.copyOfRange(10, 10 + payloadLen),
        )
    }
}

/**
 * Byte-oriented receive state machine mirroring aRacerWiFiCANService.b.
 *
 * A truncated frame leaves the parser mid-frame and the next frame is
 * swallowed until a checksum failure resynchronises. [resyncOnHeader] is an
 * opt-in deviation that restarts at a fresh F8 01 C0 boundary.
 */
class FrameParser(
    private val onFrame: (ByteArray) -> Unit,
    private val resyncOnHeader: Boolean = false,
) {
    private val buf = ByteArray(FrameCodec.FRAME_LEN)
    private var pos = 0
    private var frameLen = 0xff
    private var acc = 0
    val stats = ParserStats()

    fun reset() {
        pos = 0
        frameLen = 0xff
        acc = 0
    }

    fun feed(chunk: ByteArray) {
        for (raw in chunk) {
            val b = FrameCodec.u8(raw)
            stats.bytes++

            if (resyncOnHeader && pos > 2 && b == FrameCodec.MAGIC) {
                stats.headerResyncs++
                reset()
            }

            val headerOk = (pos != 0 || b == FrameCodec.MAGIC) &&
                (pos != 1 || b == FrameCodec.PROTOCOL) &&
                (pos != 2 || b == FrameCodec.FRAME_TYPE)
            if (!headerOk) {
                stats.discarded++
                if (pos != 0) stats.midFrameResets++
                reset()
                continue
            }
            buf[pos] = b.toByte()
            if (pos == 3) {
                frameLen = b + 4
            } else if (pos == frameLen) {
                if (((256 - acc) and 0xff) == b) {
                    stats.frames++
                    onFrame(buf.copyOf(pos + 1))
                } else {
                    stats.checksumErrors++
                }
                reset()
                continue
            }
            if (pos >= 2) acc += b
            pos++
        }
    }
}
