import 'dart:async';
import 'dart:math' as math;

import 'telemetry_snapshot.dart';
import 'telemetry_source.dart';

class SimulatorTelemetrySource implements TelemetrySource {
  SimulatorTelemetrySource({
    this.interval = const Duration(milliseconds: 80),
    DateTime Function()? clock,
  }) : _clock = clock ?? DateTime.now;

  final Duration interval;
  final DateTime Function() _clock;
  final _controller = StreamController<TelemetrySnapshot>.broadcast();
  Timer? _timer;
  double _phase = 0;
  TelemetrySnapshot? _latest;
  TelemetrySourceState _state = TelemetrySourceState.idle;
  int _sequence = 0;

  @override
  String get id => 'simulator';

  @override
  String get label => 'Simulator';

  @override
  TelemetrySourceState get state => _state;

  @override
  TelemetrySnapshot? get latest => _latest;

  @override
  Stream<TelemetrySnapshot> get snapshots => _controller.stream;

  @override
  Future<void> start() async {
    if (_timer != null) return;
    _state = TelemetrySourceState.connecting;
    _timer = Timer.periodic(interval, (_) => _tick());
    _state = TelemetrySourceState.connected;
    _tick();
  }

  void _tick() {
    _phase += 0.07;
    final rpm = 4200 + 2200 * math.sin(_phase) + 700 * math.sin(_phase * 2.3);
    final snapshot = TelemetrySnapshot.fromValues(
      {
        'RPM': rpm.clamp(900, 16000).toDouble(),
        'SPEED': (72 + 38 * math.sin(_phase * 0.6)).clamp(0, 400).toDouble(),
        'TPS_Percent': (45 + 30 * math.sin(_phase * 0.8))
            .clamp(0, 100)
            .toDouble(),
        'AFR_Target': 13.1 + 0.7 * math.sin(_phase * 0.31),
        'T_Eng': 88 + 6 * math.sin(_phase * 0.17),
        'T_Air': 31 + 2 * math.sin(_phase * 0.12),
        'Volt_Batt': 14.2 + 0.15 * math.sin(_phase * 0.5),
        'SA': 26 + 12 * math.sin(_phase * 0.55),
        'Gear': (1 + (_sequence ~/ 40) % 6).toDouble(),
      },
      capturedAt: _clock(),
      sourceId: id,
    );
    _sequence++;
    _latest = snapshot;
    _state = TelemetrySourceState.connected;
    _controller.add(snapshot);
  }

  @override
  Future<void> stop() async {
    _timer?.cancel();
    _timer = null;
    _state = TelemetrySourceState.idle;
  }

  @override
  Future<void> dispose() async {
    await stop();
    await _controller.close();
  }
}
