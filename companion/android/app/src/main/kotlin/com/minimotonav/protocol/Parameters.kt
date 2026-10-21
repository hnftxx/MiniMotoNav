package com.minimotonav.protocol

/**
 * Parameter table extracted from the decrypted ControlFile_S3.dat.
 *
 * value = clamp((raw - offset) / divisor, min, max)
 * raw   = round(clamp(value, min, max) * divisor + offset)
 *
 * Live telemetry is big-endian with [Parameter.width] bytes; SDO/table fields
 * are little-endian.
 */
data class Parameter(
    val name: String,
    val absid: Int,
    val width: Int,
    val divisor: Double,
    val offset: Double,
    val max: Double,
    val min: Double,
    val unit: String,
)

object Parameters {
    const val WIDTH_BYTE = 1
    const val WIDTH_U16 = 2
    const val WIDTH_U32 = 4

    val ALL: List<Parameter> = listOf(
        Parameter("AFR_Target", 20, 1, 10.0, 0.0, 25.5, 0.0, "AFR"),
        Parameter("Fuel_BL", 21, 1, 1.28, 128.0, 99.21, -100.0, "%"),
        Parameter("Fuel_CL", 22, 2, 5.12, 512.0, 98.08, -100.0, "%"),
        Parameter("Eng_Run_Timer", 23, 2, 1.0, 0.0, 65535.0, 0.0, "sec."),
        Parameter("T_Air", 24, 1, 1.0, 28.0, 227.0, -28.0, "\u00b0C"),
        Parameter("SA", 25, 2, 4.0, 1024.0, 127.0, -180.0, "SA"),
        Parameter("RPM", 26, 2, 1.0, 0.0, 20000.0, 0.0, "rpm"),
        Parameter("TPS_AD", 27, 2, 1.0, 0.0, 65535.0, 0.0, "A/D count"),
        Parameter("TPS_indx", 28, 1, 1.0, 0.0, 255.0, 0.0, "count"),
        Parameter("Volt_Batt", 29, 1, 10.0, 0.0, 25.5, 0.0, "V"),
        Parameter("Inj_Angle", 30, 1, -0.25, 105.0, 420.0, -296.0, "CA"),
        Parameter("ECU_Run_Min", 31, 1, 1.0, 0.0, 60.0, 0.0, "Minute"),
        Parameter("ECU_Run_Hour", 32, 2, 1.0, 0.0, 65535.0, 0.0, "Hour"),
        Parameter("T_Eng", 33, 1, 1.0, 28.0, 227.0, -28.0, "\u00b0C"),
        Parameter("TPS_Percent", 34, 1, 2.55, 0.0, 100.0, 0.0, "%"),
        Parameter("Cyl1_Comn_FuelPW", 35, 2, 250.0, 0.0, 262.14, 0.0, "ms"),
        Parameter("RPM_Freeze", 36, 1, 0.02, 0.0, 12750.0, 0.0, "rpm"),
        Parameter("T_Air_Freeze", 37, 1, 1.0, 28.0, 227.0, -28.0, "\u00b0C"),
        Parameter("TPS_Freeze", 38, 1, 1.0, 0.0, 255.0, 0.0, "count"),
        Parameter("T_Eng_Freeze", 39, 1, 1.0, 28.0, 227.0, -28.0, "\u00b0C"),
        Parameter("NBO2_BL_Freeze", 40, 1, 1.0, 0.0, 255.0, 0.0, "count"),
        Parameter("NBO2_CL_Freeze", 41, 2, 5.12, 512.0, 98.08, -100.0, "%"),
        Parameter("SA_Freeze", 42, 2, 4.0, 1024.0, 127.0, -180.0, "SA"),
        Parameter("Eng_Run_Sec_Freeze", 43, 2, 1.0, 0.0, 65535.0, 0.0, "sec."),
        Parameter("P_Atm", 44, 1, 1.0, 0.0, 255.0, 0.0, "kPa"),
        Parameter("Cyl1_Eng_AP", 45, 2, 256.0, 0.0, 255.9960938, 0.0, "kPa"),
        Parameter("NBO2_Volt", 46, 2, 0.204, 0.0, 321250.0, 0.0, "mV"),
        Parameter("Total_Eng_Run_Min", 47, 1, 1.0, 0.0, 255.0, 0.0, "Min."),
        Parameter("Total_Eng_Run_Hour", 48, 2, 1.0, 0.0, 65535.0, 0.0, "Hour"),
        Parameter("Total_Fuel", 49, 4, 256000.0, 0.0, 8388.607996, 0.0, "L"),
        Parameter("Avg_Fuel", 50, 2, 71.0, 0.0, 920.0, 0.0, "L"),
    )

    private val byAbsid = ALL.associateBy { it.absid }
    private val byName = ALL.associateBy { it.name }

    fun byAbsid(absid: Int): Parameter? = byAbsid[absid]

    fun byName(name: String): Parameter? = byName[name]

    /** Names used by the dashboard read-only profile. */
    val DASHBOARD = listOf("RPM", "TPS_Percent", "T_Eng", "Volt_Batt", "SA", "AFR_Target")

    fun clamp(v: Double, min: Double, max: Double): Double {
        val c = if (v < min) min else if (v > max) max else v
        return if (c == 0.0) 0.0 else c
    }

    fun decodeValue(p: Parameter, raw: Long): Double =
        clamp((raw - p.offset) / p.divisor, p.min, p.max)

    fun decodeValue(p: Parameter, raw: Int): Double = decodeValue(p, raw.toLong())

    fun encodeValue(p: Parameter, value: Double): Long {
        val c = clamp(value, p.min, p.max)
        val raw = Math.round(c * p.divisor + p.offset)
        val ceiling = when (p.width) {
            WIDTH_U32 -> 0xffffffffL
            WIDTH_U16 -> 0xffffL
            else -> 0xffL
        }
        return when {
            raw <= 0L -> 0L
            raw > ceiling -> ceiling
            else -> raw
        }
    }

    /** Read a little-endian unsigned integer of [width] bytes. */
    fun readLE(buf: ByteArray, offset: Int, width: Int): Long {
        var v = 0L
        for (i in width - 1 downTo 0) v = v * 256 + (buf[offset + i].toInt() and 0xff)
        return v
    }

    fun writeLE(buf: ByteArray, offset: Int, width: Int, value: Long) {
        var v = if (value > 0) value else 0L
        for (i in 0 until width) {
            buf[offset + i] = (v % 256).toByte()
            v /= 256
        }
    }

    /** Read a big-endian unsigned integer (live telemetry). */
    fun readBE(buf: ByteArray, offset: Int, width: Int): Long {
        var v = 0L
        for (i in 0 until width) v = v * 256 + (buf[offset + i].toInt() and 0xff)
        return v
    }

    fun writeBE(buf: ByteArray, offset: Int, width: Int, value: Long) {
        var v = if (value > 0) value else 0L
        for (i in width - 1 downTo 0) {
            buf[offset + i] = (v % 256).toByte()
            v /= 256
        }
    }
}
