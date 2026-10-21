package com.minimotonav.protocol

/**
 * Protocol constants, object classification and operation enforcement.
 *
 * The 4-byte frame field is a CAN ID: requests use 0x600 or seq, telemetry
 * replies 0x180 or seq, SDO replies 0x580 or seq. Object IDs live in the SDO
 * payload and are little-endian.
 *
 * Three operation classes because they are not equally safe: read, subscribe
 * (volatile TPDO configuration) and write (persistent calibration). This
 * library never emits a write operation.
 */
object Objects {
    const val DEVICE_NAME = 0x1008
    const val IDENTITY = 0x1018
    const val TPDO1_COMM = 0x1800
    const val TPDO1_MAP = 0x1a00
    const val NET_STATUS = 0x2009
    const val ECU_FLAGS_A = 0x200a
    const val ECU_FLAGS_B = 0x200b
    const val FIRMWARE = 0x200f
    const val AUTH_REPLY = 0x06a0
    const val CALLBACK_REPLY = 0x06a9

    private val names = linkedMapOf(
        "DEVICE_NAME" to DEVICE_NAME,
        "IDENTITY" to IDENTITY,
        "TPDO1_COMM" to TPDO1_COMM,
        "TPDO1_MAP" to TPDO1_MAP,
        "NET_STATUS" to NET_STATUS,
        "ECU_FLAGS_A" to ECU_FLAGS_A,
        "ECU_FLAGS_B" to ECU_FLAGS_B,
        "FIRMWARE" to FIRMWARE,
        "AUTH_REPLY" to AUTH_REPLY,
        "CALLBACK_REPLY" to CALLBACK_REPLY,
    )

    fun nameOf(obj: Int): String? = names.entries.firstOrNull { it.value == obj }?.key

    fun label(obj: Int): String {
        val name = nameOf(obj)
        return "0x${obj.toString(16)}" + if (name != null) " ($name)" else ""
    }
}

object ControlIds {
    const val CONTROL = 0x680
    const val DISCONNECT = 0x681
}

object CanBases {
    const val REQUEST = 0x600
    const val SDO_REPLY = 0x580
    const val TELEMETRY = 0x180
}

enum class Operation(val wireName: String) {
    READ("read"),
    SUBSCRIBE("subscribe"),
    WRITE("write"),
}

object Operations {
    val READ_OBJECTS = setOf(
        Objects.DEVICE_NAME,
        Objects.IDENTITY,
        Objects.NET_STATUS,
        Objects.ECU_FLAGS_A,
        Objects.ECU_FLAGS_B,
        Objects.FIRMWARE,
    )

    val SUBSCRIPTION_OBJECTS = setOf(Objects.TPDO1_COMM, Objects.TPDO1_MAP)

    fun classify(obj: Int): Operation = when (obj) {
        in READ_OBJECTS -> Operation.READ
        in SUBSCRIPTION_OBJECTS -> Operation.SUBSCRIBE
        else -> Operation.WRITE
    }
}

class ReadOnlyViolation(val obj: Int, val operation: Operation) : Exception(
    if (operation == Operation.WRITE) {
        "object ${Objects.label(obj)} is not read-only and is classified as a persistent write; " +
            "this harness never transmits tuning or calibration writes"
    } else {
        "object ${Objects.label(obj)} is not read-only for a ${operation.wireName} operation; " +
            "this harness refuses to transmit tuning or write commands"
    },
)

/**
 * [authorized] exists only for message-conformance tests; transports never
 * pass it.
 */
fun assertOperation(obj: Int, operation: Operation, authorized: Boolean = false) {
    val required = Operations.classify(obj)
    if (operation == Operation.WRITE || required == Operation.WRITE) {
        if (authorized) return
        throw ReadOnlyViolation(obj, Operation.WRITE)
    }
    if (operation == required) return
    throw ReadOnlyViolation(obj, operation)
}

fun assertReadOnly(obj: Int) = assertOperation(obj, Operation.READ)

fun assertSubscription(obj: Int) = assertOperation(obj, Operation.SUBSCRIBE)

fun requestCanId(seq: Int): Int = CanBases.REQUEST or (seq and 0x7f)

fun telemetryCanId(seq: Int): Int = CanBases.TELEMETRY or (seq and 0x7f)

/** Telemetry replies: high 7 bits == 3 and node == seq. */
fun isTelemetryId(canId: Int, seq: Int): Boolean =
    (canId ushr 7) == 3 && (canId and 0x7f) == (seq and 0x7f)

/** SDO replies: high 7 bits == 11 and node == seq. */
fun isSdoReplyId(canId: Int, seq: Int): Boolean =
    (canId ushr 7) == 11 && (canId and 0x7f) == (seq and 0x7f)
