import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:moto_companion/src/telemetry/aracer_tcp_telemetry_source.dart';
import 'package:moto_companion/src/telemetry/replay_telemetry_source.dart';
import 'package:moto_companion/src/telemetry/simulator_telemetry_source.dart';
import 'package:moto_companion/src/telemetry/telemetry_snapshot.dart';
import 'package:moto_companion/src/telemetry/telemetry_source.dart';
import 'package:moto_companion/src/telemetry/telemetry_source_controller.dart';

class _FakeSource implements TelemetrySource {
  _FakeSource(this.id);

  @override
  final String id;

  final _controller = StreamController<TelemetrySnapshot>.broadcast();
  TelemetrySnapshot? _latest;
  TelemetrySourceState _state = TelemetrySourceState.idle;

  @override
  String get label => id;

  @override
  TelemetrySnapshot? get latest => _latest;

  @override
  Stream<TelemetrySnapshot> get snapshots => _controller.stream;

  @override
  TelemetrySourceState get state => _state;

  void emit(TelemetrySnapshot snapshot) {
    _latest = snapshot;
    _state = TelemetrySourceState.connected;
    _controller.add(snapshot);
  }

  @override
  Future<void> start() async {
    _state = TelemetrySourceState.connected;
  }

  @override
  Future<void> stop() async {
    _state = TelemetrySourceState.idle;
  }

  @override
  Future<void> dispose() async {
    await _controller.close();
  }
}

TelemetrySnapshot _snapshot(double rpm, {String source = 'test'}) {
  return TelemetrySnapshot.fromValues(
    {'RPM': rpm},
    capturedAt: DateTime(2026),
    sourceId: source,
  );
}

void main() {
  test('snapshot rejects invalid and out-of-range sensor values', () {
    final snapshot = TelemetrySnapshot.fromValues(
      {'RPM': double.nan, 'TPS_Percent': 140, 'AFR_Target': 13.2, 'Gear': 3},
      capturedAt: DateTime(2026),
      sourceId: 'test',
    );

    expect(snapshot.rpm, isNull);
    expect(snapshot.throttlePercent, isNull);
    expect(snapshot.afr, 13.2);
    expect(snapshot.gear, 3);
    expect(snapshot.isValid(TelemetryField.afr), isTrue);
    expect(snapshot.isValid(TelemetryField.rpm), isFalse);
    expect(snapshot.raw['TPS_Percent'], 140);
  });

  test('switching sources clears old values and ignores late events', () async {
    final first = _FakeSource('first');
    final second = _FakeSource('second');
    final controller = TelemetrySourceController(first);

    await controller.start();
    first.emit(_snapshot(5000, source: 'first'));
    await Future<void>.delayed(Duration.zero);
    expect(controller.latest?.rpm, 5000);

    await controller.switchTo(second);
    expect(controller.latest, isNull);
    expect(controller.source, same(second));

    first.emit(_snapshot(9000, source: 'first'));
    await Future<void>.delayed(Duration.zero);
    expect(controller.latest, isNull);

    second.emit(_snapshot(7000, source: 'second'));
    await Future<void>.delayed(Duration.zero);
    expect(controller.latest?.rpm, 7000);
    expect(controller.latest?.sourceId, 'second');
  });

  test(
    'ECU disconnect marks stale values disconnected, never simulated',
    () async {
      final updates = StreamController<Map<String, Object?>>.broadcast();
      final source = AracerTcpTelemetrySource(updates: updates.stream);
      final controller = TelemetrySourceController(source);

      await controller.start();
      updates.add({
        'type': 'telemetry',
        'values': {'RPM': 6200.0, 'TPS_Percent': 38.0},
      });
      await Future<void>.delayed(Duration.zero);
      expect(controller.latest?.rpm, 6200);
      expect(source.state, TelemetrySourceState.connected);

      updates.add({'type': 'status', 'state': 'closed'});
      await Future<void>.delayed(Duration.zero);
      expect(controller.latest?.status, TelemetrySnapshotStatus.disconnected);
      expect(controller.latest?.rpm, 6200);
      expect(controller.latest?.sourceId, 'aracer-tcp');
      expect(controller.state, TelemetrySourceState.disconnected);

      updates.add({
        'type': 'telemetry',
        'values': {'RPM': 7000.0},
      });
      await Future<void>.delayed(Duration.zero);
      expect(controller.latest?.status, TelemetrySnapshotStatus.live);
      expect(controller.latest?.rpm, 7000);

      controller.dispose();
      await source.dispose();
      await updates.close();
    },
  );

  test('ECU telemetry becomes stale after the stale window', () async {
    final updates = StreamController<Map<String, Object?>>.broadcast();
    var now = DateTime(2026);
    final source = AracerTcpTelemetrySource(
      updates: updates.stream,
      staleAfter: const Duration(milliseconds: 10),
      clock: () => now,
    );
    final controller = TelemetrySourceController(source);

    await controller.start();
    updates.add({
      'type': 'telemetry',
      'values': {'RPM': 4200.0},
    });
    await Future<void>.delayed(Duration.zero);
    expect(controller.stale, isFalse);

    now = now.add(const Duration(seconds: 3));
    await Future<void>.delayed(const Duration(milliseconds: 320));
    expect(controller.latest?.status, TelemetrySnapshotStatus.stale);
    expect(controller.state, TelemetrySourceState.stale);
    expect(controller.latest?.rpm, 4200);

    controller.dispose();
    await updates.close();
  });

  test('simulator and replay sources emit ordered snapshots', () async {
    final simulator = SimulatorTelemetrySource(
      interval: const Duration(milliseconds: 5),
    );
    final seen = <TelemetrySnapshot>[];
    final sub = simulator.snapshots.listen(seen.add);
    await simulator.start();
    await Future<void>.delayed(const Duration(milliseconds: 30));
    await simulator.stop();
    await sub.cancel();
    expect(seen, isNotEmpty);
    expect(seen.last.rpm, isNotNull);
    expect(seen.last.sourceId, 'simulator');
    await simulator.dispose();

    final frames = [_snapshot(1000), _snapshot(2000), _snapshot(3000)];
    final replay = ReplayTelemetrySource(
      frames,
      interval: const Duration(milliseconds: 5),
    );
    final replayed = <TelemetrySnapshot>[];
    final replaySub = replay.snapshots.listen(replayed.add);
    await replay.start();
    await Future<void>.delayed(const Duration(milliseconds: 40));
    await replaySub.cancel();
    expect(replayed.map((s) => s.rpm), [1000, 2000, 3000]);
    await replay.dispose();
  });
}
