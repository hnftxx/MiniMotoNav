import 'dart:async';

import 'telemetry_snapshot.dart';
import 'telemetry_source.dart';

class ReplayTelemetrySource implements TelemetrySource {
  ReplayTelemetrySource(
    List<TelemetrySnapshot> frames, {
    this.interval = const Duration(milliseconds: 100),
    this.loop = false,
  }) : _frames = List.unmodifiable(frames);

  final List<TelemetrySnapshot> _frames;
  final Duration interval;
  final bool loop;
  final _controller = StreamController<TelemetrySnapshot>.broadcast();
  Timer? _timer;
  int _index = 0;
  TelemetrySnapshot? _latest;
  TelemetrySourceState _state = TelemetrySourceState.idle;

  @override
  String get id => 'replay';

  @override
  String get label => 'Replay';

  @override
  TelemetrySourceState get state => _state;

  @override
  TelemetrySnapshot? get latest => _latest;

  @override
  Stream<TelemetrySnapshot> get snapshots => _controller.stream;

  @override
  Future<void> start() async {
    if (_timer != null) return;
    if (_frames.isEmpty) {
      _state = TelemetrySourceState.error;
      return;
    }
    _index = 0;
    _state = TelemetrySourceState.connecting;
    _timer = Timer.periodic(interval, (_) => _tick());
    _tick();
  }

  void _tick() {
    if (_index >= _frames.length) {
      if (loop) {
        _index = 0;
      } else {
        _timer?.cancel();
        _timer = null;
        _state = TelemetrySourceState.disconnected;
        return;
      }
    }
    final frame = _frames[_index++];
    final snapshot = frame.status == TelemetrySnapshotStatus.live
        ? frame
        : frame.copyWith(status: TelemetrySnapshotStatus.live);
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
