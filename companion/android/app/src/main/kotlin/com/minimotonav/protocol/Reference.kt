package com.minimotonav.protocol

import java.time.Instant
import java.time.ZoneOffset
import java.util.Locale

/**
 * Reference-profile (RaceChrono bridge) parser and decoder, ported from
 * timliudev/Aracer_TCPdecode2Racechrono at c3c4b25 (decoode.py).
 *
 * Not the X Tune app profile: standard CANopen SDO at 0x602, telemetry at 0x182
 * with data[0] = frame index 1..N, and a 0x70B watchdog.
 */
object ReferenceIds {
    const val SDO = 0x602
    const val TELEMETRY = 0x182
    const val WATCHDOG = 0x70b
    const val CONTROL = 0x680
}

/** CAN ID from a 19-byte wire frame (4-byte big-endian field at offset 4). */
fun frameCanId(message: ByteArray): Int? {
    if (message.size < 8) return null
    return ((message[4].toInt() and 0xff) shl 24) or
        ((message[5].toInt() and 0xff) shl 16) or
        ((message[6].toInt() and 0xff) shl 8) or
        (message[7].toInt() and 0xff)
}

data class PdoField(val offset: Int, val width: Int)

data class PdoMap(val fields: LinkedHashMap<Int, PdoField>, val cycleBytes: Int)

/** Telemetry layout fallback used by decoode.py when config.txt is absent. */
val FALLBACK_FIELDS: LinkedHashMap<Int, PdoField> = linkedMapOf(
    111 to PdoField(0, 1), 112 to PdoField(1, 1),
    113 to PdoField(2, 1), 114 to PdoField(3, 2),
    116 to PdoField(5, 1), 117 to PdoField(6, 1),
    118 to PdoField(7, 2), 119 to PdoField(9, 1),
    120 to PdoField(10, 1), 121 to PdoField(11, 1),
    122 to PdoField(12, 2), 123 to PdoField(14, 1),
    115 to PdoField(15, 1), 124 to PdoField(16, 2),
    26 to PdoField(24, 2), 34 to PdoField(26, 1),
    51 to PdoField(27, 2), 108 to PdoField(29, 1),
    126 to PdoField(30, 1), 29 to PdoField(31, 1),
    33 to PdoField(32, 1), 24 to PdoField(33, 1),
    55 to PdoField(34, 2), 45 to PdoField(36, 2),
)

object ReferenceVars {
    const val GPS_UTC_hh = 111
    const val GPS_UTC_mm = 112
    const val GPS_UTC_ss = 113
    const val GPS_UTC_ms = 114
    const val GPS_Valid = 115
    const val GPS_Lat_deg = 116
    const val GPS_Lat_min = 117
    const val GPS_Lat_mmmm = 118
    const val GPS_Lat_NS = 119
    const val GPS_Lon_deg = 120
    const val GPS_Lon_min = 121
    const val GPS_Lon_mmmm = 122
    const val GPS_Lon_EW = 123
    const val GPS_Speed = 124
    const val RPM = 26
    const val TPS_Percent = 34
    const val Vehicle_Speed = 51
    const val TC_Lean_Angle = 108
    const val TC_FR_Rate = 126
    const val Volt_Batt = 29
    const val T_Eng = 33
    const val T_Air = 24
    const val AFR_WBO2 = 55
    const val Cyl1_Eng_AP = 45
}

data class Scale(val gain: Double, val offset: Double)

/** value = (raw - offset) / gain. */
val REFERENCE_SCALE: Map<Int, Scale> = mapOf(
    26 to Scale(1.0, 0.0),
    34 to Scale(2.55, 0.0),
    51 to Scale(256.0, 0.0),
    108 to Scale(1.0, 128.0),
    126 to Scale(1.28, 0.0),
    29 to Scale(10.0, 0.0),
    33 to Scale(1.0, 28.0),
    24 to Scale(1.0, 28.0),
    55 to Scale(100.0, 0.0),
    45 to Scale(256.0, 0.0),
)

private fun hexBytes(line: String): ByteArray? {
    if (line.length % 2 != 0) return null
    val out = ByteArray(line.length / 2)
    for (i in out.indices) {
        val hi = Character.digit(line[i * 2], 16)
        val lo = Character.digit(line[i * 2 + 1], 16)
        if (hi < 0 || lo < 0) return null
        out[i] = ((hi shl 4) or lo).toByte()
    }
    return out
}

/**
 * Mirrors decoode.py `_parse_pdo_map`: only 38-character hex lines whose CAN ID
 * is 0x602 and whose payload starts 23 00 1A are TPDO1 mapping writes.
 */
fun parsePdoMap(text: String): PdoMap {
    val fields = linkedMapOf<Int, PdoField>()
    var offset = 0
    for (raw in text.split(Regex("\\r?\\n"))) {
        val line = raw.trim()
        if (line.length != 38) continue
        val bytes = hexBytes(line) ?: continue
        if (bytes.size != 19) continue
        if (frameCanId(bytes) != ReferenceIds.SDO) continue
        val d = bytes.copyOfRange(10, 18)
        if ((d[0].toInt() and 0xff) != 0x23 ||
            (d[1].toInt() and 0xff) != 0x00 ||
            (d[2].toInt() and 0xff) != 0x1a
        ) continue
        val varid = ((d[5].toInt() and 0xff) shl 8) or (d[6].toInt() and 0xff)
        val width = (d[4].toInt() and 0xff) ushr 3
        if (width !in 1..4) continue
        fields[varid] = PdoField(offset, width)
        offset += width
    }
    return PdoMap(fields, offset)
}

/** Last byte covered by a scaled or GPS variable, in 7-byte frame units. */
fun computeEmitIndex(fields: Map<Int, PdoField>): Int {
    var last = 0
    for ((varid, field) in fields) {
        val tracked = REFERENCE_SCALE.containsKey(varid) || (varid > 110 && varid < 125)
        if (tracked && field.offset + field.width > last) last = field.offset + field.width
    }
    return if (last == 0) 0 else (last - 1) / 7 + 1
}

data class ReferenceFrames(val watchdog: List<ByteArray>, val init: List<ByteArray>)

/** Splits a config.txt into its [watchdog] and [init] frame lists. */
fun parseReferenceConfig(text: String): ReferenceFrames {
    val watchdog = mutableListOf<ByteArray>()
    val init = mutableListOf<ByteArray>()
    var current: String? = null
    val header = Regex("^\\[(\\w+)]$")
    for (raw in text.split(Regex("\\r?\\n"))) {
        val line = raw.trim()
        if (line.isEmpty()) continue
        val match = header.matchEntire(line)
        if (match != null) {
            current = match.groupValues[1].takeIf { it == "watchdog" || it == "init" }
            continue
        }
        if (current == null || line.length != 38) continue
        val bytes = hexBytes(line) ?: continue
        if (bytes.size != 19) continue
        when (current) {
            "watchdog" -> watchdog.add(bytes)
            "init" -> init.add(bytes)
        }
    }
    return ReferenceFrames(watchdog, init)
}

data class ReferenceProfile(
    val fields: LinkedHashMap<Int, PdoField>,
    val cycleBytes: Int,
    val watchdog: List<ByteArray>,
    val init: List<ByteArray>,
) {
    companion object {
        fun parse(text: String): ReferenceProfile {
            val map = parsePdoMap(text)
            val frames = parseReferenceConfig(text)
            val fields = if (map.fields.isNotEmpty()) map.fields else FALLBACK_FIELDS
            val cycleBytes = if (map.fields.isNotEmpty()) map.cycleBytes else 54
            return ReferenceProfile(fields, cycleBytes, frames.watchdog, frames.init)
        }
    }
}

/** Six full frames plus a seventh fragment and one non-telemetry frame. */
val REFERENCE_SAMPLE_FRAMES = listOf(
    "f801c00e0000018200080103252402581706e3",
    "f801c00e0000018200080209854e780d0e1026",
    "f801c00e0000018200080345410020753371e5",
    "f801c00e00000182000804fa6d7f2bbaff27b2",
    "f801c00e00000182000805ff64008d704309f6",
    "f801c00e00000182000806fb5b2c09290b1cc6",
    "f801c00e00000182000807000000100000038d",
    "f801c00e000001820006887e538300ff0000ce",
)

private class Acceleration(private val now: () -> Long) {
    private var time = now()
    private var speed = 0.0

    fun calculate(value: Double): String {
        val current = now()
        val dt = (current - time) / 1000.0
        val diff = value - speed
        speed = value
        time = current
        if (dt == 0.0) return "0.000"
        return format(diff / dt, 3)
    }
}

private class BearingCalculator(private val now: () -> Long) {
    private var prevLat: Double? = null
    private var prevLon: Double? = null
    private var lastOutput = "0.00"
    private var emaValue: Double? = null

    fun update(nmeaLat: String, latNs: String, nmeaLon: String, lonEw: String, speedKnots: Double): String {
        if (speedKnots < 1) return lastOutput
        if (nmeaLat.isEmpty() || nmeaLon.isEmpty()) return lastOutput
        var lat = nmeaToDecimal(nmeaLat)
        var lon = nmeaToDecimal(nmeaLon)
        if (latNs == "S") lat = -lat
        if (lonEw == "W") lon = -lon
        val pLat = prevLat
        val pLon = prevLon
        if (pLat == null || pLon == null) {
            prevLat = lat
            prevLon = lon
            return lastOutput
        }
        val lat1 = pLat * Math.PI / 180
        val lon1 = pLon * Math.PI / 180
        val lat2 = lat * Math.PI / 180
        val lon2 = lon * Math.PI / 180
        val dlon = lon2 - lon1
        val y = Math.sin(dlon) * Math.cos(lat2)
        val x = Math.cos(lat1) * Math.sin(lat2) - Math.sin(lat1) * Math.cos(lat2) * Math.cos(dlon)
        var bearing = Math.atan2(y, x) * 180 / Math.PI
        bearing = (bearing + 360) % 360
        prevLat = lat
        prevLon = lon
        emaValue = emaValue?.let { 0.3 * bearing + 0.7 * it } ?: bearing
        lastOutput = format(emaValue!!, 2)
        return lastOutput
    }
}

private fun nmeaToDecimal(text: String): Double {
    if (text.isEmpty()) return 0.0
    val value = text.toDoubleOrNull() ?: 0.0
    val degrees = value.toLong()
    return degrees + (value - degrees * 100) / 60
}

private fun format(value: Double, decimals: Int): String =
    String.format(Locale.US, "%.${decimals}f", value)

private fun pad(value: Long, width: Int): String = value.toString().padStart(width, '0')

private fun nmeaChecksum(line: String): String {
    var sum = 0
    for (ch in line) sum = sum xor ch.code
    return (sum and 0xff).toString(16).uppercase(Locale.US).padStart(2, '0')
}

private fun charOr(raw: Long, fallback: Char): String =
    if (raw in 32..126) raw.toInt().toChar().toString() else fallback.toString()

private fun tireCircumferenceMeters(): Double =
    ((120.0 * 80.0 * 2.0) / 1000.0 + 12.0 * 2.54) * Math.PI / 100.0

private fun gearRatio(): Double = (39.0 / 12.0) * (40.0 / 13.0)

/**
 * Reference-profile cycle decoder. Accepts complete 19-byte frames and returns
 * the RC3/NMEA text on the emit frame. [now] is injectable for deterministic
 * tests.
 */
class ReferenceDecoder(
    private val profile: ReferenceProfile,
    private val now: () -> Long = System::currentTimeMillis,
) {
    val fields: Map<Int, PdoField> = profile.fields
    val cycleBytes: Int = profile.cycleBytes
    val emitIndex: Int = computeEmitIndex(fields)
    private val buf = ByteArray(maxOf(cycleBytes, 64))
    private var count = 0
    private val bearing = BearingCalculator(now)
    private val speedAcc = Acceleration(now)
    private val rpmAcc = Acceleration(now)
    private val reductionAcc = Acceleration(now)

    fun raw(varid: Int): Long? {
        val field = fields[varid] ?: return null
        if (field.offset + field.width > buf.size) return null
        var value = 0L
        for (i in 0 until field.width) value = value * 256 + (buf[field.offset + i].toInt() and 0xff)
        return value
    }

    fun phy(varid: Int, fallback: Double = 0.0): Double {
        val raw = raw(varid) ?: return fallback
        val scale = REFERENCE_SCALE[varid] ?: Scale(1.0, 0.0)
        return (raw - scale.offset) / scale.gain
    }

    fun decodeFrame(message: ByteArray): String {
        if (message.size < 11) return ""
        if (frameCanId(message) != ReferenceIds.TELEMETRY) return ""
        val index = message[10].toInt() and 0xff
        if (index == 0 || index > 16) return ""
        val dlc = message[9].toInt() and 0xff
        val take = maxOf(dlc - 1, 0)
        val base = (index - 1) * 7
        for (k in 0 until take) {
            val at = 11 + k
            if (at >= message.size) break
            if (base + k < buf.size) buf[base + k] = message[at]
        }
        if (index != emitIndex) return ""
        return buildOutput()
    }

    private fun buildOutput(): String {
        count = (count + 1) and 0xffff
        val r = { id: Int -> raw(id) ?: 0L }

        val utcHour = pad(r(ReferenceVars.GPS_UTC_hh), 2)
        val utcMinute = pad(r(ReferenceVars.GPS_UTC_mm), 2)
        val utcSecond = pad(r(ReferenceVars.GPS_UTC_ss), 2)
        val utcMillis = pad(r(ReferenceVars.GPS_UTC_ms), 3)
        val latDeg = pad(r(ReferenceVars.GPS_Lat_deg), 2)
        val latMin = pad(r(ReferenceVars.GPS_Lat_min), 2)
        val latSec = pad(r(ReferenceVars.GPS_Lat_mmmm), 4)
        val latNs = charOr(r(ReferenceVars.GPS_Lat_NS), 'N')
        val lonDeg = pad(r(ReferenceVars.GPS_Lon_deg), 2)
        val lonMin = pad(r(ReferenceVars.GPS_Lon_min), 2)
        val lonSec = pad(r(ReferenceVars.GPS_Lon_mmmm), 4)
        val lonEw = charOr(r(ReferenceVars.GPS_Lon_EW), 'E')
        val gpsValid = charOr(r(ReferenceVars.GPS_Valid), 'V')
        val gpsSpeedKmh = r(ReferenceVars.GPS_Speed).toDouble()
        val gpsSpeed = format(gpsSpeedKmh * 0.539956803, 3)

        val rpm = phy(ReferenceVars.RPM).toInt()
        val tps = phy(ReferenceVars.TPS_Percent)
        val vehicleSpeed = phy(ReferenceVars.Vehicle_Speed)
        val leanAngle = phy(ReferenceVars.TC_Lean_Angle)
        val frRate = phy(ReferenceVars.TC_FR_Rate)
        val voltBatt = phy(ReferenceVars.Volt_Batt)
        val tEng = phy(ReferenceVars.T_Eng)
        val tAir = phy(ReferenceVars.T_Air)
        val afr = phy(ReferenceVars.AFR_WBO2)
        val mapKpa = phy(ReferenceVars.Cyl1_Eng_AP)

        val hasGps = gpsValid == "A"
        var nmeaTime = ""
        var date = ""
        var bearingText = "0.00"
        val lat = "$latDeg$latMin.$latSec"
        val lon = "$lonDeg$lonMin.$lonSec"
        if (hasGps) {
            nmeaTime = "$utcHour$utcMinute$utcSecond.$utcMillis"
            val d = Instant.ofEpochMilli(now()).atZone(ZoneOffset.UTC)
            date = pad(d.dayOfMonth.toLong(), 2) +
                pad(d.monthValue.toLong(), 2) +
                pad((d.year % 100).toLong(), 2)
            bearingText = bearing.update(lat, latNs, lon, lonEw, gpsSpeedKmh * 0.539956803)
        }

        val ms2 = speedAcc.calculate(gpsSpeedKmh * 0.277777778)
        val rps2 = rpmAcc.calculate(if (rpm != 0) rpm / 60.0 else 0.0)
        val circumference = tireCircumferenceMeters()
        val denom = if (circumference != 0.0) (gpsSpeedKmh * 1000 / 60) / circumference else 0.0
        val reductionRatio = if (denom != 0.0) format(rpm / denom, 3) else "0.000"
        val irrs2 = reductionAcc.calculate(reductionRatio.toDouble())
        val alphaRatio = format(reductionRatio.toDouble() / gearRatio(), 3)

        val result = StringBuilder()
        if (hasGps) {
            val gga = "GNGGA,$nmeaTime,$lat,$latNs,$lon,$lonEw,1,,,,M,,M,,"
            val rmc = "GNRMC,$nmeaTime,$gpsValid,$lat,$latNs,$lon,$lonEw," +
                "$gpsSpeed,$bearingText,$date,,,A"
            result.append("$").append(gga).append("*").append(nmeaChecksum(gga)).append("\n")
            result.append("$").append(rmc).append("*").append(nmeaChecksum(rmc)).append("\n")
        }

        val rc3 = "RC3,$nmeaTime,$count,,,,,,," +
            "$rpm,${format(tps, 1)},${format(vehicleSpeed, 2)},${format(leanAngle, 0)}," +
            "${format(afr, 2)},${format(mapKpa, 1)},${format(voltBatt, 1)}," +
            "${format(tEng, 0)},${format(tAir, 0)},${format(frRate, 1)}," +
            "$reductionRatio,$alphaRatio,$irrs2,$ms2,$rps2"
        result.append("$").append(rc3).append("*").append(nmeaChecksum(rc3)).append("\n")
        return result.toString()
    }
}
