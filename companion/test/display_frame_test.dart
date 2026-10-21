import 'package:flutter_test/flutter_test.dart';
import 'package:moto_companion/src/display/display_frame.dart';
import 'package:moto_companion/src/navigation/navigation_state.dart';
import 'package:moto_companion/src/telemetry/telemetry_snapshot.dart';

void main() {
  final now = DateTime.utc(2026, 10, 21, 12);

  test('navigation frame survives without ECU telemetry', () {
    final frame = DisplayFrame.merge(
      telemetry: null,
      navigation: const NavigationState(
        maneuver: NavigationManeuver.left,
        distanceToManeuverM: 120,
        routeValid: true,
        gpsFix: true,
      ),
      ecuConnected: false,
      now: now,
    );

    expect(frame.navigation.maneuver, NavigationManeuver.left);
    expect(frame.navigation.routeValid, isTrue);
    expect(frame.telemetryStale, isTrue);
    expect(frame.values, isEmpty);
  });

  test('fresh telemetry is live only while the ECU link is up', () {
    final snapshot = TelemetrySnapshot(
      capturedAt: now.subtract(const Duration(milliseconds: 300)),
      rpm: 4200,
      validFields: const {TelemetryField.rpm},
      sourceId: 'ecu',
    );

    final live = DisplayFrame.merge(
      telemetry: snapshot,
      navigation: const NavigationState(),
      ecuConnected: true,
      now: now,
    );
    final disconnected = DisplayFrame.merge(
      telemetry: snapshot,
      navigation: const NavigationState(),
      ecuConnected: false,
      now: now,
    );

    expect(live.telemetryStale, isFalse);
    expect(live.values['RPM'], 4200);
    expect(disconnected.telemetryStale, isTrue);
    expect(disconnected.values['RPM'], 4200);
  });

  test('old data stays visible but marked stale', () {
    final snapshot = TelemetrySnapshot(
      capturedAt: now.subtract(const Duration(seconds: 5)),
      speedKph: 63,
      validFields: const {TelemetryField.speedKph},
    );
    final frame = DisplayFrame.merge(
      telemetry: snapshot,
      navigation: const NavigationState(),
      ecuConnected: true,
      now: now,
    );
    expect(frame.telemetryStale, isTrue);
    expect(frame.values['SPEED'], 63);
  });
}
