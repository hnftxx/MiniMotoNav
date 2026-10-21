package com.minimotonav.protocol

const val TELEMETRY_SELECTOR_BASE = 16384
const val CALIBRATION_SELECTOR_BASE = 12288

data class Selector(val width: Int, val absid: Int)

fun buildSelector(width: Int, absid: Int, base: Int = TELEMETRY_SELECTOR_BASE): Long {
    val low = (((width and 0x1f) shl 3) and 0xff).toLong()
    val high = ((absid + base).toLong() shl 16) and 0xffff0000L
    return (low or high) and 0xffffffffL
}

fun parseSelector(selector: Long, base: Int = TELEMETRY_SELECTOR_BASE): Selector {
    val s = selector and 0xffffffffL
    return Selector(
        width = ((s ushr 3) and 0x1f).toInt(),
        absid = ((s ushr 16) and 0xffff).toInt() - base,
    )
}

private fun u16le(v: Int): ByteArray = byteArrayOf((v and 0xff).toByte(), ((v ushr 8) and 0xff).toByte())

private fun u32le(v: Long): ByteArray {
    val s = v and 0xffffffffL
    return byteArrayOf(
        (s and 0xff).toByte(),
        ((s ushr 8) and 0xff).toByte(),
        ((s ushr 16) and 0xff).toByte(),
        ((s ushr 24) and 0xff).toByte(),
    )
}

private fun sdoData(cmd: Int, obj: Int, sub: Int, tail: ByteArray = ByteArray(0)): ByteArray {
    val out = ByteArray(8)
    out[0] = cmd.toByte()
    u16le(obj).copyInto(out, 1)
    out[3] = sub.toByte()
    tail.copyInto(out, 4, 0, minOf(tail.size, 4))
    return out
}

/** Short read: 0x23/0x27/0x2B/0x2F for lengths 4/3/2/1. */
fun sdoReadData(obj: Int, sub: Int, len: Int): ByteArray {
    require(len in 1..4) { "short read length must be 1..4, got $len" }
    return sdoData(((4 - len) shl 2) or 0x23, obj, sub, byteArrayOf(0, 0, 0, 0))
}

/** Long read: 0xC6, object, sub, requested length LE32. */
fun sdoLongReadData(obj: Int, sub: Int, len: Int): ByteArray =
    sdoData(0xc6, obj, sub, u32le(len.toLong()))

/** Short write: 0x43/0x47/0x4B/0x4F for lengths 4/3/2/1. */
fun sdoWriteData(obj: Int, sub: Int, value: Long, len: Int = 4): ByteArray {
    require(len in 1..4) { "short write length must be 1..4, got $len" }
    val bytes = u32le(value)
    for (i in len until 4) bytes[i] = 0
    return sdoData(((4 - len) shl 2) or 0x43, obj, sub, bytes)
}

private fun requestFrame(seq: Int, data: ByteArray): ByteArray =
    FrameCodec.encode(requestCanId(seq), data)

fun readObjectFrame(seq: Int, obj: Int, sub: Int, len: Int): ByteArray {
    assertOperation(obj, Operation.READ)
    return requestFrame(seq, if (len <= 4) sdoReadData(obj, sub, len) else sdoLongReadData(obj, sub, len))
}

/** DeviceName request: CAN ID 0x600, long read 0x1008 sub 0, length 10. */
fun deviceNameFrame(seq: Int = 0): ByteArray =
    readObjectFrame(seq, Objects.DEVICE_NAME, 0, 10)

fun identityFrame(seq: Int = 0, sub: Int = 0): ByteArray =
    readObjectFrame(seq, Objects.IDENTITY, sub, 8)

/** Subscribe one parameter into TPDO1 slot [index] (1..255). */
fun subscribeFrame(seq: Int, index: Int, width: Int, absid: Int): ByteArray {
    assertSubscription(Objects.TPDO1_MAP)
    require(index in 1..0xff) { "subscribe index out of range: $index" }
    return requestFrame(seq, sdoData(0x43, Objects.TPDO1_MAP, index, u32le(buildSelector(width, absid))))
}

/** Commit the mapping list: 0x4F 00 1A 00 count. */
fun subscribeCloseFrame(seq: Int, count: Int): ByteArray {
    assertSubscription(Objects.TPDO1_MAP)
    return requestFrame(seq, sdoWriteData(Objects.TPDO1_MAP, 0, count.toLong(), 1))
}

/** Set TPDO1 event timer in ms: 0x4B 00 18 05 ms LE16. */
fun setStreamTimerFrame(seq: Int, ms: Int): ByteArray {
    assertSubscription(Objects.TPDO1_COMM)
    return requestFrame(seq, sdoWriteData(Objects.TPDO1_COMM, 5, ms.toLong(), 2))
}

/** Full read-only subscription sequence for a parameter list. */
fun subscribeAllFrames(params: List<Parameter>, seq: Int = 0): List<ByteArray> {
    require(params.size <= 0xff) { "too many parameters for one subscription" }
    val frames = params.mapIndexed { i, p -> subscribeFrame(seq, i + 1, p.width, p.absid) }.toMutableList()
    frames.add(subscribeCloseFrame(seq, params.size))
    return frames
}

data class SdoReply(
    val command: Int,
    val obj: Int,
    val sub: Int,
    val ack: Boolean,
    val length: Int? = null,
    val value: Long? = null,
)

/** Decodes conventional CANopen-style expedited SDO replies. */
fun decodeSdoReply(payload: ByteArray): SdoReply {
    require(payload.size >= 4) { "SDO reply shorter than 4 bytes" }
    val command = payload[0].toInt() and 0xff
    val obj = (payload[1].toInt() and 0xff) or ((payload[2].toInt() and 0xff) shl 8)
    val sub = payload[3].toInt() and 0xff
    if ((command and 0xe0) != 0x60) return SdoReply(command, obj, sub, ack = false)
    val unused = (command ushr 2) and 3
    val length = 4 - unused
    if (length == 0) return SdoReply(command, obj, sub, ack = true)
    var value = 0L
    for (i in 0 until length) {
        value += (payload[4 + i].toInt() and 0xff).toLong() shl (8 * i)
    }
    if (command == 0x60 && value == 0L) return SdoReply(command, obj, sub, ack = true)
    return SdoReply(command, obj, sub, ack = false, length = length, value = value)
}

data class TelemetrySample(
    val absid: Int,
    val raw: Long,
    val value: Double,
    val parameter: Parameter,
)

/**
 * Reassembles telemetry blocks for a fixed subscription list. Bytes are
 * big-endian, matching ByteBuffer.getChar()/getInt().
 */
class TelemetryAssembler(private val params: List<Parameter>, private val seq: Int = 0) {
    private val bytes = mutableListOf<Int>()
    private var started = false

    init {
        require(params.size <= 0xff) { "too many parameters for one subscription" }
    }

    fun reset() {
        bytes.clear()
        started = false
    }

    fun push(frame: DecodedFrame): List<TelemetrySample>? {
        if (!isTelemetryId(frame.canId, seq)) return null
        val payload = frame.payload
        if (payload.size < 2) return null
        val marker = payload[0].toInt() and 0xff
        val index = marker and 0x7f
        val last = (marker and 0x80) != 0

        if (index == 1) {
            bytes.clear()
            started = true
        }
        if (!started) return null
        for (i in 1 until payload.size) bytes.add(payload[i].toInt() and 0xff)
        if (!last) return null

        val buf = ByteArray(bytes.size) { bytes[it].toByte() }
        val out = mutableListOf<TelemetrySample>()
        var offset = 0
        for (p in params) {
            if (offset + p.width > buf.size) break
            val raw = Parameters.readBE(buf, offset, p.width)
            offset += p.width
            out.add(TelemetrySample(p.absid, raw, Parameters.decodeValue(p, raw), p))
        }
        reset()
        return out
    }

    fun snapshot(results: List<TelemetrySample>?): Map<String, Double> {
        val out = linkedMapOf<String, Double>()
        for (r in results ?: emptyList()) {
            val p = Parameters.byAbsid(r.absid) ?: continue
            out[p.name] = r.value
        }
        return out
    }
}
