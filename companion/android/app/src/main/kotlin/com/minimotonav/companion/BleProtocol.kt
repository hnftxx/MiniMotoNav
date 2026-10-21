package com.minimotonav.companion

import java.util.UUID

object BleProtocol {
    val SERVICE_UUID: UUID = UUID.fromString("b5e10001-6d6e-4d4e-8000-6d696e696d6f")
    val TELEMETRY_UUID: UUID = UUID.fromString("b5e10002-6d6e-4d4e-8000-6d696e696d6f")
    val NAVIGATION_UUID: UUID = UUID.fromString("b5e10003-6d6e-4d4e-8000-6d696e696d6f")
    val CONTROL_UUID: UUID = UUID.fromString("b5e10004-6d6e-4d4e-8000-6d696e696d6f")
    val STATUS_UUID: UUID = UUID.fromString("b5e10005-6d6e-4d4e-8000-6d696e696d6f")
    val CAPABILITY_UUID: UUID = UUID.fromString("b5e10006-6d6e-4d4e-8000-6d696e696d6f")
    val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    const val VERSION = 1
    const val DEFAULT_MTU = 247
    const val PACKET_OVERHEAD = 14

    fun maxWritePayload(mtu: Int): Int = (mtu - 3).coerceAtLeast(20)

    fun maxPayloadBytes(mtu: Int): Int = (maxWritePayload(mtu) - PACKET_OVERHEAD).coerceAtLeast(1)
}
