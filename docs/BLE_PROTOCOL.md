# GODSPEED BLE Protocol

Status: version 1 contract. Host codec, Android transport, and the ESP32-S3
NimBLE GATT server are implemented and compile; real-hardware timing and the
phone-to-board link are not yet verified.

This contract is project-owned. It carries no aRacer identifiers, credentials,
tuning data, or write commands to the ECU. Android is the BLE central; the
ESP32-S3 display is the peripheral. aRacer communication stays on the separate
read-only TCP link.

## Service and characteristics

Base UUID: `b5e10000-6d6e-4d4e-8000-6d696e696d6f`

| Name | UUID | Direction | Properties |
| --- | --- | --- | --- |
| Service | `b5e10001-6d6e-4d4e-8000-6d696e696d6f` | - | primary |
| Telemetry | `b5e10002-6d6e-4d4e-8000-6d696e696d6f` | phone to display | write, write without response |
| Navigation | `b5e10003-6d6e-4d4e-8000-6d696e696d6f` | phone to display | write, write without response |
| Control | `b5e10004-6d6e-4d4e-8000-6d696e696d6f` | phone to display | write |
| Status | `b5e10005-6d6e-4d4e-8000-6d696e696d6f` | display to phone | notify |
| Capability | `b5e10006-6d6e-4d4e-8000-6d696e696d6f` | display to phone | read, notify |

The peripheral advertises the service UUID in its advertising data. Android
must filter on that UUID and must not scan indefinitely. Discovery, MTU
negotiation (247 requested), and the capability exchange complete before any
telemetry write.

## Packet format

All multi-byte integers are little-endian except where a payload explicitly
states otherwise.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | protocol version (`1`) |
| 1 | 1 | message type |
| 2 | 1 | flags |
| 3 | 1 | sequence |
| 4 | 4 | timestamp, Unix epoch seconds, unsigned |
| 8 | 2 | data age, milliseconds, unsigned |
| 10 | 2 | payload length, unsigned |
| 12 | N | payload |
| 12+N | 2 | CRC-16/CCITT-FALSE over bytes `0..12+N-1` |

Header size is 12 bytes and CRC is 2 bytes, so one GATT write of `L` bytes
carries at most `L - 14` payload bytes. With MTU 247 and ATT write overhead 3,
the default payload budget is 233 bytes.

Message types:

| Value | Name | Purpose |
| --- | --- | --- |
| 1 | Telemetry | normalized live values |
| 2 | Navigation | current maneuver and route summary |
| 3 | Control | subscribe, unsubscribe, ping |
| 4 | Status | peripheral acknowledgement or fault |
| 5 | Capability | version, MTU, supported message types |
| 6 | Route geometry | compact ordered route points |

Flags:

| Bit | Mask | Meaning |
| --- | --- | --- |
| 0 | `0x01` | first fragment |
| 1 | `0x02` | last fragment |
| 2 | `0x04` | payload marked valid by sender |
| 3 | `0x08` | acknowledgement requested |
| 4 | `0x10` | fragmented message |

## Fragmentation and reassembly

A payload larger than the negotiated write budget is split into ordered
fragments with the same type, sequence, timestamp, and data age. The first
fragment sets `first`, the final fragment sets `last`, and every fragment sets
`fragmented`. Each fragment carries its own CRC and is written in order.

BLE GATT writes are ordered and reliable on one connection. The receiver keys
reassembly by `(type, sequence)`, starts only on `first`, appends only to an
existing key, and emits only on `last`. Orphan continuations are discarded.
Sequence wrap and an interrupted transfer must discard the incomplete key.
There is no fragment-index field because GATT ordering supplies it.

## Telemetry payload

Fixed 16 bytes. The validity mask enables each field independently; receivers
must not substitute zero for an absent field.

| Offset | Size | Field | Scale | Valid range |
| --- | --- | --- | --- | --- |
| 0 | 2 | validity mask | - | bits 0..8 |
| 2 | 2 | RPM | 1 rpm | 0..20000 |
| 4 | 2 | speed | 0.1 km/h | 0..400 km/h |
| 6 | 1 | throttle | 0.5 percent | 0..100 percent |
| 7 | 1 | AFR | 0.1 AFR | 5..25 |
| 8 | 2 | engine temperature, signed | 0.1 C | -60..300 C |
| 10 | 2 | intake temperature, signed | 0.1 C | -60..300 C |
| 12 | 2 | battery voltage | 1 mV | 0..30 V |
| 14 | 1 | ignition advance, signed | 0.5 deg | -60..80 deg |
| 15 | 1 | gear, signed | 1 gear | 0..8 |

Validity bits: 0 RPM, 1 speed, 2 throttle, 3 AFR, 4 engine temperature,
5 intake temperature, 6 battery voltage, 7 ignition advance, 8 gear.

Data age is the age of the underlying ECU sample at send time. Android must
not present telemetry older than its stale window as live, and a disconnect
must retain the last values as explicitly stale rather than fall back to
simulator values.

## Navigation payload

Minimum 14 bytes plus a UTF-8 road name of 0..40 bytes.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | flags: bit 0 route valid, bit 1 GPS fix, bit 2 maneuver present, bit 3 ETA present |
| 2 | 1 | maneuver enum |
| 3 | 2 | distance to maneuver, metres |
| 5 | 2 | remaining distance, 0.1 km |
| 7 | 4 | ETA, Unix epoch seconds |
| 11 | 1 | GPS accuracy, 0.5 m |
| 12 | 1 | road name byte length |
| 13 | 1 | route point count, informational |
| 14 | N | UTF-8 road name |

Maneuver enum: 0 none, 1 straight, 2 slight left, 3 left, 4 sharp left,
5 slight right, 6 right, 7 sharp right, 8 U-turn, 9 roundabout, 10 arrive.

## Route geometry payload

Two-byte point count followed by 8 bytes per point: signed 32-bit latitude
delta in 1e-7 degrees, then signed 32-bit longitude delta in 1e-7 degrees.
Deltas are relative to the previous point, with the first delta relative to
zero. Maximum 512 points per message. Senders should prefer a new message over
oversized geometry. This is geometry, never a raster image.

## Capability payload

`version(1)`, `max_mtu(1)`, `capabilities(1)`, reserved(1),
`max_payload(2)`, `name_length(2)`, UTF-8 name.

Capability bits: 0 telemetry, 1 navigation, 2 control, 3 route geometry.

## Control and status

Control payload: `command(1)`, `value(2)`. Commands: 1 subscribe,
2 unsubscribe, 3 ping. A control message must not carry ECU tuning or
calibration operations.

Status payload is peripheral-defined: status code, acknowledged sequence,
and optional UTF-8 detail. Status must report protocol version mismatch,
unsupported message type, CRC failure, and reassembly timeout.

## Reconnect and synchronization

Android reconnects with exponential backoff, re-discovers the service,
re-negotiates MTU, and retransmits only the latest valid telemetry and
navigation snapshots. The display keeps rendering its last valid frame while
the link is down and marks it stale; it never freezes the UI. A new connection
starts with a capability exchange, then a current-state snapshot, then normal
rate-limited updates.

## Compatibility

The version byte is mandatory. Version 1 receivers reject other versions.
Adding a field requires a new message type or a version bump; field order and
scales in this document are frozen for version 1.

## Firmware transport (compiled, hardware-unverified)

`src/platform/ble_server.h/.cpp` implements the peripheral side with
NimBLE-Arduino 2.5.1: the version 1 service and all five characteristics,
fragment reassembly keyed by `(type, sequence)` with a 5 s timeout, CRC and
version validation, status notifications for `ack`, `version_mismatch`,
`unsupported_type`, `crc_error`, `reassembly_timeout`, `payload_overflow`, and
`malformed_payload`, control handling (`subscribe`, `unsubscribe`, `ping`), and
a capability response carrying protocol version, MTU, capability bits,
maximum payload size, and the advertised name. MTU changes refresh the
capability payload; the server advertises `GODSPEED` with the service UUID
and restarts advertising on disconnect.

`src/platform/esp32_main.cpp` drains decoded telemetry and navigation frames
into `MotoApp` each loop, forwards link-state changes to `setBleLinked()`, and
calls `tickBle()` so the stale window advances without new frames.

`tests/ble_protocol_tests.cpp` verifies CRC rejection, version rejection,
truncation, route deltas, display-feed staleness, and byte-for-byte parity with
Dart `BleProtocol.encode` output for telemetry and navigation. CTest passes 6/6.
`pio run -e esp32s3` compiles the firmware.

Not verified: MTU negotiation on a real phone, notification throughput and
display latency, end-to-end link behaviour, and any timing claim. No ESP32-S3
hardware or Android device has been attached to this host.
