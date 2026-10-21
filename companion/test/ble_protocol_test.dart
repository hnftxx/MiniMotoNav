import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:moto_companion/src/ble/ble_protocol.dart';
import 'package:moto_companion/src/navigation/navigation_state.dart';
import 'package:moto_companion/src/telemetry/telemetry_snapshot.dart';

void main() {
  final timestamp =
      DateTime.utc(2026, 10, 21, 12).millisecondsSinceEpoch ~/ 1000;

  test('header round trip and crc validation', () {
    final message = BleMessage(
      type: BleProtocol.typeControl,
      sequence: 42,
      timestampSeconds: timestamp,
      payload: BleProtocol.encodeControl(BleProtocol.controlPing),
    );
    final encoded = BleProtocol.encode(message);
    final decoded = BleProtocol.decode(encoded);

    expect(decoded.type, BleProtocol.typeControl);
    expect(decoded.sequence, 42);
    expect(decoded.timestampSeconds, timestamp);
    expect(decoded.payload, message.payload);

    encoded[encoded.length - 1] ^= 0xFF;
    expect(
      () => BleProtocol.decode(encoded),
      throwsA(isA<BleProtocolException>()),
    );
  });

  test('telemetry encodes every valid field and rejects corrupted lengths', () {
    final snapshot = TelemetrySnapshot.fromValues(
      {
        'RPM': 6250,
        'SPEED': 112.4,
        'TPS_Percent': 43.5,
        'AFR_Target': 13.2,
        'T_Eng': 91.3,
        'T_Air': 28.7,
        'Volt_Batt': 13.84,
        'SA': 21.5,
        'Gear': 4,
      },
      capturedAt: DateTime.fromMillisecondsSinceEpoch(timestamp * 1000),
      sourceId: 'test',
    );
    final packet = BleProtocol.decode(
      BleProtocol.encode(
        BleMessage(
          type: BleProtocol.typeTelemetry,
          sequence: 1,
          timestampSeconds: timestamp,
          payload: BleProtocol.encodeTelemetry(snapshot),
        ),
      ),
    );
    final decoded = BleProtocol.decodeTelemetry(packet);

    expect(decoded.rpm, 6250);
    expect(decoded.speedKph, closeTo(112.4, 0.01));
    expect(decoded.throttlePercent, 43.5);
    expect(decoded.afr, closeTo(13.2, 0.01));
    expect(decoded.engineTempC, closeTo(91.3, 0.01));
    expect(decoded.intakeTempC, closeTo(28.7, 0.01));
    expect(decoded.batteryVolts, closeTo(13.84, 0.001));
    expect(decoded.ignitionAdvanceDeg, 21.5);
    expect(decoded.gear, 4);
    expect(decoded.validFields.length, 9);
  });

  test('mtu-sized fragmentation reassembles into one message', () {
    final payload = Uint8List.fromList(List.generate(120, (i) => i));
    final message = BleMessage(
      type: BleProtocol.typeRouteGeometry,
      sequence: 7,
      timestampSeconds: timestamp,
      payload: payload,
    );
    final fragments = BleProtocol.fragment(message, maxWriteLength: 20);

    expect(fragments.length, greaterThan(1));
    for (final fragment in fragments) {
      expect(fragment.length, lessThanOrEqualTo(20));
    }
    expect(BleProtocol.decode(fragments.first).isFirst, isTrue);
    expect(BleProtocol.decode(fragments.last).isLast, isTrue);

    final reassembler = BleReassembler();
    BleMessage? result;
    for (final fragment in fragments) {
      result = reassembler.accept(BleProtocol.decode(fragment));
    }
    expect(result, isNotNull);
    expect(result!.type, BleProtocol.typeRouteGeometry);
    expect(result.payload, payload);
  });

  test('reassembler ignores orphan continuation fragments', () {
    final fragment = BleProtocol.decode(
      BleProtocol.fragment(
        BleMessage(
          type: BleProtocol.typeTelemetry,
          sequence: 9,
          timestampSeconds: timestamp,
          payload: Uint8List(40),
        ),
        maxWriteLength: 20,
      )[1],
    );
    expect(BleReassembler().accept(fragment), isNull);
  });

  test('navigation and capability payloads round trip', () {
    final navigation = NavigationState(
      maneuver: NavigationManeuver.roundabout,
      distanceToManeuverM: 320,
      roadName: 'Hauptstraße',
      eta: DateTime.fromMillisecondsSinceEpoch(timestamp * 1000),
      remainingDistanceKm: 12.4,
      gpsAccuracyM: 3.5,
      routeValid: true,
      gpsFix: true,
    );
    final navPacket = BleProtocol.decode(
      BleProtocol.encode(
        BleMessage(
          type: BleProtocol.typeNavigation,
          sequence: 3,
          timestampSeconds: timestamp,
          payload: BleProtocol.encodeNavigation(navigation),
        ),
      ),
    );
    final decoded = BleProtocol.decodeNavigation(navPacket);
    expect(decoded.maneuver, NavigationManeuver.roundabout);
    expect(decoded.distanceToManeuverM, 320);
    expect(decoded.roadName, 'Hauptstraße');
    expect(decoded.remainingDistanceKm, closeTo(12.4, 0.01));
    expect(decoded.gpsAccuracyM, 3.5);
    expect(decoded.routeValid, isTrue);
    expect(decoded.gpsFix, isTrue);

    final capability = BleCapability(
      maxMtu: 247,
      capabilities:
          BleProtocol.capabilityTelemetry |
          BleProtocol.capabilityNavigation |
          BleProtocol.capabilityControl |
          BleProtocol.capabilityRouteGeometry,
      maxPayload: 233,
      deviceName: 'MiniMotoNav Display',
    );
    final capPacket = BleProtocol.decode(
      BleProtocol.encode(
        BleMessage(
          type: BleProtocol.typeCapability,
          sequence: 1,
          timestampSeconds: timestamp,
          payload: BleProtocol.encodeCapability(capability),
        ),
      ),
    );
    final decodedCapability = BleProtocol.decodeCapability(capPacket);
    expect(decodedCapability.deviceName, 'MiniMotoNav Display');
    expect(decodedCapability.maxMtu, 247);
    expect(
      decodedCapability.supports(BleProtocol.capabilityRouteGeometry),
      isTrue,
    );
  });

  test('compact route geometry uses delta encoding', () {
    final points = const [
      RoutePoint(52.520008, 13.404954),
      RoutePoint(52.520100, 13.405100),
      RoutePoint(52.520250, 13.405300),
    ];
    final payload = BleProtocol.encodeRouteGeometry(points);
    expect(payload.length, 2 + points.length * 8);
    final decoded = BleProtocol.decodeRouteGeometry(
      BleProtocol.decode(
        BleProtocol.encode(
          BleMessage(
            type: BleProtocol.typeRouteGeometry,
            sequence: 2,
            timestampSeconds: timestamp,
            payload: payload,
          ),
        ),
      ),
    );
    expect(decoded.length, points.length);
    for (var i = 0; i < points.length; i++) {
      expect(decoded[i].latitude, closeTo(points[i].latitude, 1e-7));
      expect(decoded[i].longitude, closeTo(points[i].longitude, 1e-7));
    }
  });
}
