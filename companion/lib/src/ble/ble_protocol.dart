import 'dart:convert';
import 'dart:typed_data';

import '../navigation/navigation_state.dart';
import '../telemetry/telemetry_snapshot.dart';

class BleProtocol {
  static const int version = 1;

  static const String serviceUuid = 'b5e10001-6d6e-4d4e-8000-6d696e696d6f';
  static const String telemetryCharacteristicUuid =
      'b5e10002-6d6e-4d4e-8000-6d696e696d6f';
  static const String navigationCharacteristicUuid =
      'b5e10003-6d6e-4d4e-8000-6d696e696d6f';
  static const String controlCharacteristicUuid =
      'b5e10004-6d6e-4d4e-8000-6d696e696d6f';
  static const String statusCharacteristicUuid =
      'b5e10005-6d6e-4d4e-8000-6d696e696d6f';
  static const String capabilityCharacteristicUuid =
      'b5e10006-6d6e-4d4e-8000-6d696e696d6f';

  static const int headerLength = 12;
  static const int crcLength = 2;
  static const int packetOverhead = headerLength + crcLength;
  static const int defaultMtu = 247;
  static const int defaultWriteLength = defaultMtu - 3;

  static const int typeTelemetry = 1;
  static const int typeNavigation = 2;
  static const int typeControl = 3;
  static const int typeStatus = 4;
  static const int typeCapability = 5;
  static const int typeRouteGeometry = 6;

  static const int flagFirst = 1 << 0;
  static const int flagLast = 1 << 1;
  static const int flagValid = 1 << 2;
  static const int flagAckRequired = 1 << 3;
  static const int flagFragment = 1 << 4;

  static const int capabilityTelemetry = 1 << 0;
  static const int capabilityNavigation = 1 << 1;
  static const int capabilityControl = 1 << 2;
  static const int capabilityRouteGeometry = 1 << 3;

  static const int controlSubscribe = 1;
  static const int controlUnsubscribe = 2;
  static const int controlPing = 3;

  static int maxPayloadForWriteLength(int writeLength) {
    final payload = writeLength - packetOverhead;
    if (payload < 1) {
      throw ArgumentError.value(
        writeLength,
        'writeLength',
        'must be at least $packetOverhead bytes',
      );
    }
    return payload;
  }

  static Uint8List encode(BleMessage message) {
    if (message.payload.length > 0xFFFF) {
      throw ArgumentError('payload exceeds 65535 bytes');
    }
    final bytes = Uint8List(packetOverhead + message.payload.length);
    final view = ByteData.sublistView(bytes);
    view.setUint8(0, version);
    view.setUint8(1, message.type);
    view.setUint8(2, message.flags);
    view.setUint8(3, message.sequence & 0xFF);
    view.setUint32(4, message.timestampSeconds & 0xFFFFFFFF, Endian.little);
    view.setUint16(8, message.dataAgeMs, Endian.little);
    view.setUint16(10, message.payload.length, Endian.little);
    bytes.setRange(
      headerLength,
      headerLength + message.payload.length,
      message.payload,
    );
    final crc = crc16(bytes, 0, headerLength + message.payload.length);
    view.setUint16(headerLength + message.payload.length, crc, Endian.little);
    return bytes;
  }

  static BlePacket decode(Uint8List bytes) {
    if (bytes.length < packetOverhead) {
      throw const BleProtocolException('packet too short');
    }
    final view = ByteData.sublistView(bytes);
    final packetVersion = view.getUint8(0);
    if (packetVersion != version) {
      throw BleProtocolException('unsupported protocol version $packetVersion');
    }
    final type = view.getUint8(1);
    final flags = view.getUint8(2);
    final sequence = view.getUint8(3);
    final timestampSeconds = view.getUint32(4, Endian.little);
    final dataAgeMs = view.getUint16(8, Endian.little);
    final payloadLength = view.getUint16(10, Endian.little);
    if (bytes.length != packetOverhead + payloadLength) {
      throw const BleProtocolException('payload length mismatch');
    }
    final crcOffset = headerLength + payloadLength;
    final expected = view.getUint16(crcOffset, Endian.little);
    final actual = crc16(bytes, 0, crcOffset);
    if (expected != actual) {
      throw const BleProtocolException('crc mismatch');
    }
    return BlePacket(
      type: type,
      flags: flags,
      sequence: sequence,
      timestampSeconds: timestampSeconds,
      dataAgeMs: dataAgeMs,
      payload: Uint8List.sublistView(bytes, headerLength, crcOffset),
    );
  }

  static List<Uint8List> fragment(
    BleMessage message, {
    required int maxWriteLength,
  }) {
    final maxPayload = maxPayloadForWriteLength(maxWriteLength);
    if (message.payload.length <= maxPayload) {
      return [
        encode(
          BleMessage(
            type: message.type,
            flags: message.flags | flagValid,
            sequence: message.sequence,
            timestampSeconds: message.timestampSeconds,
            dataAgeMs: message.dataAgeMs,
            payload: message.payload,
          ),
        ),
      ];
    }
    final chunks = <Uint8List>[];
    for (
      var offset = 0;
      offset < message.payload.length;
      offset += maxPayload
    ) {
      final end = (offset + maxPayload)
          .clamp(0, message.payload.length)
          .toInt();
      final first = offset == 0;
      final last = end == message.payload.length;
      var flags = message.flags | flagFragment | flagValid;
      if (first) flags |= flagFirst;
      if (last) flags |= flagLast;
      chunks.add(
        encode(
          BleMessage(
            type: message.type,
            flags: flags,
            sequence: message.sequence,
            timestampSeconds: message.timestampSeconds,
            dataAgeMs: message.dataAgeMs,
            payload: Uint8List.sublistView(message.payload, offset, end),
          ),
        ),
      );
    }
    return chunks;
  }

  static Uint8List encodeTelemetry(TelemetrySnapshot snapshot) {
    final bytes = Uint8List(16);
    final view = ByteData.sublistView(bytes);
    var flags = 0;
    if (snapshot.rpm != null) flags |= 1 << 0;
    if (snapshot.speedKph != null) flags |= 1 << 1;
    if (snapshot.throttlePercent != null) flags |= 1 << 2;
    if (snapshot.afr != null) flags |= 1 << 3;
    if (snapshot.engineTempC != null) flags |= 1 << 4;
    if (snapshot.intakeTempC != null) flags |= 1 << 5;
    if (snapshot.batteryVolts != null) flags |= 1 << 6;
    if (snapshot.ignitionAdvanceDeg != null) flags |= 1 << 7;
    if (snapshot.gear != null) flags |= 1 << 8;
    view.setUint16(0, flags, Endian.little);
    view.setUint16(
      2,
      (snapshot.rpm ?? 0).round().clamp(0, 20000).toInt(),
      Endian.little,
    );
    view.setUint16(
      4,
      ((snapshot.speedKph ?? 0) * 10).round().clamp(0, 4000).toInt(),
      Endian.little,
    );
    view.setUint8(
      6,
      ((snapshot.throttlePercent ?? 0) * 2).round().clamp(0, 200).toInt(),
    );
    view.setUint8(7, ((snapshot.afr ?? 0) * 10).round().clamp(0, 255).toInt());
    view.setInt16(
      8,
      ((snapshot.engineTempC ?? 0) * 10).round().clamp(-32768, 32767).toInt(),
      Endian.little,
    );
    view.setInt16(
      10,
      ((snapshot.intakeTempC ?? 0) * 10).round().clamp(-32768, 32767).toInt(),
      Endian.little,
    );
    view.setUint16(
      12,
      ((snapshot.batteryVolts ?? 0) * 1000).round().clamp(0, 65535).toInt(),
      Endian.little,
    );
    view.setInt8(
      14,
      ((snapshot.ignitionAdvanceDeg ?? 0) * 2).round().clamp(-128, 127).toInt(),
    );
    view.setInt8(15, (snapshot.gear ?? 0).round().clamp(0, 8).toInt());
    return bytes;
  }

  static TelemetrySnapshot decodeTelemetry(
    BlePacket packet, {
    TelemetrySnapshotStatus status = TelemetrySnapshotStatus.live,
  }) {
    if (packet.payload.length != 16) {
      throw const BleProtocolException('telemetry payload must be 16 bytes');
    }
    final view = ByteData.sublistView(packet.payload);
    final flags = view.getUint16(0, Endian.little);
    final values = <String, double>{};
    if (flags & (1 << 0) != 0) {
      values['RPM'] = view.getUint16(2, Endian.little).toDouble();
    }
    if (flags & (1 << 1) != 0) {
      values['SPEED'] = view.getUint16(4, Endian.little) / 10;
    }
    if (flags & (1 << 2) != 0) {
      values['TPS_Percent'] = view.getUint8(6) / 2;
    }
    if (flags & (1 << 3) != 0) {
      values['AFR_Target'] = view.getUint8(7) / 10;
    }
    if (flags & (1 << 4) != 0) {
      values['T_Eng'] = view.getInt16(8, Endian.little) / 10;
    }
    if (flags & (1 << 5) != 0) {
      values['T_Air'] = view.getInt16(10, Endian.little) / 10;
    }
    if (flags & (1 << 6) != 0) {
      values['Volt_Batt'] = view.getUint16(12, Endian.little) / 1000;
    }
    if (flags & (1 << 7) != 0) {
      values['SA'] = view.getInt8(14) / 2;
    }
    if (flags & (1 << 8) != 0) {
      values['Gear'] = view.getInt8(15).toDouble();
    }
    return TelemetrySnapshot.fromValues(
      values,
      capturedAt: DateTime.fromMillisecondsSinceEpoch(
        packet.timestampSeconds * 1000,
      ),
      sourceId: 'ble',
      status: status,
    );
  }

  static Uint8List encodeNavigation(NavigationState state) {
    final road = utf8.encode(state.roadName);
    final roadLength = road.length.clamp(0, 40).toInt();
    final bytes = Uint8List(14 + roadLength);
    final view = ByteData.sublistView(bytes);
    var flags = 0;
    if (state.routeValid) flags |= 1 << 0;
    if (state.gpsFix) flags |= 1 << 1;
    if (state.maneuver != NavigationManeuver.none) flags |= 1 << 2;
    if (state.eta != null) flags |= 1 << 3;
    view.setUint16(0, flags, Endian.little);
    view.setUint8(2, state.maneuver.index);
    view.setUint16(
      3,
      state.distanceToManeuverM.round().clamp(0, 65535).toInt(),
      Endian.little,
    );
    view.setUint16(
      5,
      (state.remainingDistanceKm * 10).round().clamp(0, 65535).toInt(),
      Endian.little,
    );
    view.setUint32(
      7,
      state.eta == null ? 0 : state.eta!.millisecondsSinceEpoch ~/ 1000,
      Endian.little,
    );
    view.setUint8(11, (state.gpsAccuracyM * 2).round().clamp(0, 255).toInt());
    view.setUint8(12, roadLength);
    view.setUint8(13, state.routeGeometry.length.clamp(0, 255).toInt());
    bytes.setRange(14, 14 + roadLength, road);
    return bytes;
  }

  static NavigationState decodeNavigation(BlePacket packet) {
    if (packet.payload.length < 14) {
      throw const BleProtocolException('navigation payload too short');
    }
    final view = ByteData.sublistView(packet.payload);
    final flags = view.getUint16(0, Endian.little);
    final roadLength = view.getUint8(12);
    if (packet.payload.length < 14 + roadLength) {
      throw const BleProtocolException('navigation road name truncated');
    }
    final etaSeconds = view.getUint32(7, Endian.little);
    return NavigationState(
      routeValid: flags & (1 << 0) != 0,
      gpsFix: flags & (1 << 1) != 0,
      maneuver:
          NavigationManeuver.values[view
              .getUint8(2)
              .clamp(0, NavigationManeuver.values.length - 1)
              .toInt()],
      distanceToManeuverM: view.getUint16(3, Endian.little).toDouble(),
      remainingDistanceKm: view.getUint16(5, Endian.little) / 10,
      eta: etaSeconds == 0
          ? null
          : DateTime.fromMillisecondsSinceEpoch(etaSeconds * 1000),
      gpsAccuracyM: view.getUint8(11) / 2,
      roadName: utf8.decode(
        packet.payload.sublist(14, 14 + roadLength),
        allowMalformed: true,
      ),
    );
  }

  static Uint8List encodeRouteGeometry(List<RoutePoint> points) {
    final count = points.length.clamp(0, 512).toInt();
    final bytes = Uint8List(2 + count * 8);
    final view = ByteData.sublistView(bytes);
    view.setUint16(0, count, Endian.little);
    var previousLat = 0;
    var previousLon = 0;
    for (var i = 0; i < count; i++) {
      final lat = (points[i].latitude * 1e7).round();
      final lon = (points[i].longitude * 1e7).round();
      view.setInt32(2 + i * 8, lat - previousLat, Endian.little);
      view.setInt32(6 + i * 8, lon - previousLon, Endian.little);
      previousLat = lat;
      previousLon = lon;
    }
    return bytes;
  }

  static List<RoutePoint> decodeRouteGeometry(BlePacket packet) {
    if (packet.payload.length < 2) {
      throw const BleProtocolException('route geometry too short');
    }
    final view = ByteData.sublistView(packet.payload);
    final count = view.getUint16(0, Endian.little);
    if (packet.payload.length != 2 + count * 8) {
      throw const BleProtocolException('route geometry length mismatch');
    }
    var lat = 0;
    var lon = 0;
    final points = <RoutePoint>[];
    for (var i = 0; i < count; i++) {
      lat += view.getInt32(2 + i * 8, Endian.little);
      lon += view.getInt32(6 + i * 8, Endian.little);
      points.add(RoutePoint(lat / 1e7, lon / 1e7));
    }
    return points;
  }

  static Uint8List encodeCapability(BleCapability capability) {
    final name = utf8.encode(capability.deviceName);
    final length = name.length.clamp(0, 32).toInt();
    final bytes = Uint8List(8 + length);
    final view = ByteData.sublistView(bytes);
    view.setUint8(0, version);
    view.setUint8(1, capability.maxMtu.clamp(23, 255).toInt());
    view.setUint8(2, capability.capabilities & 0xFF);
    view.setUint8(3, 0);
    view.setUint16(
      4,
      capability.maxPayload.clamp(0, 65535).toInt(),
      Endian.little,
    );
    view.setUint16(6, length, Endian.little);
    bytes.setRange(8, 8 + length, name);
    return bytes;
  }

  static BleCapability decodeCapability(BlePacket packet) {
    if (packet.payload.length < 8) {
      throw const BleProtocolException('capability payload too short');
    }
    final view = ByteData.sublistView(packet.payload);
    final nameLength = view.getUint16(6, Endian.little);
    if (packet.payload.length < 8 + nameLength) {
      throw const BleProtocolException('capability name truncated');
    }
    return BleCapability(
      maxMtu: view.getUint8(1),
      capabilities: view.getUint8(2),
      maxPayload: view.getUint16(4, Endian.little),
      deviceName: utf8.decode(
        packet.payload.sublist(8, 8 + nameLength),
        allowMalformed: true,
      ),
    );
  }

  static Uint8List encodeControl(int command, {int value = 0}) {
    final bytes = Uint8List(3);
    bytes[0] = command & 0xFF;
    ByteData.sublistView(bytes).setUint16(1, value & 0xFFFF, Endian.little);
    return bytes;
  }

  static ({int command, int value}) decodeControl(BlePacket packet) {
    if (packet.payload.length < 3) {
      throw const BleProtocolException('control payload too short');
    }
    return (
      command: packet.payload[0],
      value: ByteData.sublistView(packet.payload).getUint16(1, Endian.little),
    );
  }

  static int crc16(Uint8List bytes, int start, int end) {
    var crc = 0xFFFF;
    for (var i = start; i < end; i++) {
      crc ^= bytes[i] << 8;
      for (var bit = 0; bit < 8; bit++) {
        crc = crc & 0x8000 != 0 ? (crc << 1) ^ 0x1021 : crc << 1;
        crc &= 0xFFFF;
      }
    }
    return crc;
  }
}

class BleMessage {
  const BleMessage({
    required this.type,
    required this.sequence,
    required this.timestampSeconds,
    required this.payload,
    this.flags = 0,
    this.dataAgeMs = 0,
  });

  final int type;
  final int flags;
  final int sequence;
  final int timestampSeconds;
  final int dataAgeMs;
  final Uint8List payload;
}

class BlePacket {
  const BlePacket({
    required this.type,
    required this.flags,
    required this.sequence,
    required this.timestampSeconds,
    required this.dataAgeMs,
    required this.payload,
  });

  final int type;
  final int flags;
  final int sequence;
  final int timestampSeconds;
  final int dataAgeMs;
  final Uint8List payload;

  bool get isFirst => flags & BleProtocol.flagFirst != 0;
  bool get isLast => flags & BleProtocol.flagLast != 0;
  bool get isFragment => flags & BleProtocol.flagFragment != 0;
}

class BleProtocolException implements Exception {
  const BleProtocolException(this.message);

  final String message;

  @override
  String toString() => 'BleProtocolException: $message';
}

class BleReassembler {
  final Map<int, Uint8List> _buffers = {};
  final Map<int, BlePacket> _headers = {};

  BleMessage? accept(BlePacket packet) {
    if (!packet.isFragment) {
      return BleMessage(
        type: packet.type,
        flags: packet.flags,
        sequence: packet.sequence,
        timestampSeconds: packet.timestampSeconds,
        dataAgeMs: packet.dataAgeMs,
        payload: packet.payload,
      );
    }
    final key = (packet.type << 8) | packet.sequence;
    if (packet.isFirst) {
      _buffers[key] = Uint8List.fromList(packet.payload);
      _headers[key] = packet;
    } else {
      final existing = _buffers[key];
      if (existing == null) return null;
      final combined = Uint8List(existing.length + packet.payload.length)
        ..setRange(0, existing.length, existing)
        ..setRange(
          existing.length,
          existing.length + packet.payload.length,
          packet.payload,
        );
      _buffers[key] = combined;
    }
    if (!packet.isLast) return null;
    final payload = _buffers.remove(key);
    final header = _headers.remove(key);
    if (payload == null || header == null) return null;
    return BleMessage(
      type: header.type,
      flags: header.flags,
      sequence: header.sequence,
      timestampSeconds: header.timestampSeconds,
      dataAgeMs: header.dataAgeMs,
      payload: payload,
    );
  }

  void reset() {
    _buffers.clear();
    _headers.clear();
  }
}

class BleCapability {
  const BleCapability({
    required this.maxMtu,
    required this.capabilities,
    required this.maxPayload,
    required this.deviceName,
  });

  final int maxMtu;
  final int capabilities;
  final int maxPayload;
  final String deviceName;

  bool supports(int capability) => capabilities & capability != 0;
}
