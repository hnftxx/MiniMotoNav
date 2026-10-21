import 'dart:async';

import 'package:flutter/foundation.dart';

import 'telemetry_snapshot.dart';
import 'telemetry_source.dart';

class TelemetrySourceController extends ChangeNotifier {
  TelemetrySourceController(this._source);

  TelemetrySource _source;
  StreamSubscription<TelemetrySnapshot>? _subscription;
  TelemetrySnapshot? _latest;
  TelemetrySourceState _state = TelemetrySourceState.idle;
  int _generation = 0;
  int _freshCount = 0;
  bool _started = false;

  TelemetrySource get source => _source;

  TelemetrySnapshot? get latest => _latest;

  TelemetrySourceState get state => _state;

  int get freshCount => _freshCount;

  bool get hasData => _latest != null;

  bool get stale => _latest?.stale ?? false;

  Future<void> start() async {
    if (_started) return;
    _started = true;
    final generation = ++_generation;
    _bind(generation);
    await _source.start();
    if (generation != _generation) return;
    _state = _source.state;
    notifyListeners();
  }

  Future<void> switchTo(TelemetrySource next) async {
    if (identical(next, _source) && _started) return;
    final generation = ++_generation;
    _started = true;
    await _unbind();
    await _source.stop();
    if (generation != _generation) {
      await next.stop();
      return;
    }
    _source = next;
    _latest = null;
    _freshCount = 0;
    _state = TelemetrySourceState.idle;
    notifyListeners();
    _bind(generation);
    await next.start();
    if (generation != _generation) return;
    _state = next.state;
    notifyListeners();
  }

  Future<void> stop() async {
    ++_generation;
    _started = false;
    await _unbind();
    await _source.stop();
    _latest = null;
    _freshCount = 0;
    _state = TelemetrySourceState.idle;
    notifyListeners();
  }

  void _bind(int generation) {
    _subscription = _source.snapshots.listen(
      (snapshot) {
        if (generation != _generation) return;
        _latest = snapshot;
        if (snapshot.status == TelemetrySnapshotStatus.live) {
          _freshCount++;
        }
        _state = _stateFor(snapshot.status);
        notifyListeners();
      },
      onError: (Object error) {
        if (generation != _generation) return;
        _state = TelemetrySourceState.error;
        notifyListeners();
      },
    );
  }

  Future<void> _unbind() async {
    await _subscription?.cancel();
    _subscription = null;
  }

  TelemetrySourceState _stateFor(TelemetrySnapshotStatus status) {
    return switch (status) {
      TelemetrySnapshotStatus.live => TelemetrySourceState.connected,
      TelemetrySnapshotStatus.stale => TelemetrySourceState.stale,
      TelemetrySnapshotStatus.disconnected => TelemetrySourceState.disconnected,
      TelemetrySnapshotStatus.error => TelemetrySourceState.error,
    };
  }

  @override
  void dispose() {
    ++_generation;
    _subscription?.cancel();
    _subscription = null;
    _source.stop();
    super.dispose();
  }
}
