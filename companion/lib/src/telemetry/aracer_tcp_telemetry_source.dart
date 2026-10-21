import 'dart:async';

import '../protocol_service.dart';
import 'telemetry_snapshot.dart';
import 'telemetry_source.dart';

class AracerTcpTelemetrySource implements TelemetrySource {
  AracerTcpTelemetrySource({
    Stream<Map<String, Object?>>? updates,
    this.staleAfter = const Duration(seconds: 2),
    DateTime Function()? clock,
  }) : _updates = updates ?? ProtocolService.instance.updates,
       _clock = clock ?? DateTime.now;

  final Stream<Map<String, Object?>> _updates;
  final Duration staleAfter;
  final DateTime Function() _clock;
  final _controller = StreamController<TelemetrySnapshot>.broadcast();
  StreamSubscription<Map<String, Object?>>? _subscription;
  Timer? _staleTimer;
  TelemetrySnapshot? _latest;
  TelemetrySourceState _state = TelemetrySourceState.idle;
  int _freshCount = 0;

  @override
  String get id => 'aracer-tcp';

  @override
  String get label => 'aRacer TCP';

  @override
  TelemetrySourceState get state => _state;

  @override
  TelemetrySnapshot? get latest => _latest;

  @override
  Stream<TelemetrySnapshot> get snapshots => _controller.stream;

  int get freshCount => _freshCount;

  @override
  Future<void> start() async {
    if (_subscription != null) return;
    _state = TelemetrySourceState.connecting;
    _subscription = _updates.listen(
      _onEvent,
      onError: (Object error) => _fail(error),
    );
    _staleTimer ??= Timer.periodic(
      const Duration(milliseconds: 250),
      (_) => _checkStale(),
    );
  }

  void _onEvent(Map<String, Object?> event) {
    switch (event['type']) {
      case 'telemetry':
        final raw = event['values'];
        if (raw is! Map) return;
        final values = <String, double>{};
        for (final entry in raw.entries) {
          final value = entry.value;
          if (value is num) values['${entry.key}'] = value.toDouble();
        }
        final snapshot = TelemetrySnapshot.fromValues(
          values,
          capturedAt: _clock(),
          sourceId: id,
        );
        _latest = snapshot;
        _freshCount++;
        _state = TelemetrySourceState.connected;
        _controller.add(snapshot);
      case 'status':
        final next = event['state'];
        if (next == 'connected') {
          _state = TelemetrySourceState.connected;
        } else if (next == 'connecting') {
          _state = TelemetrySourceState.connecting;
        } else if (next == 'error') {
          _state = TelemetrySourceState.error;
          _publishStatus(TelemetrySnapshotStatus.error);
        } else if (next == 'closed') {
          _state = TelemetrySourceState.disconnected;
          _publishStatus(TelemetrySnapshotStatus.disconnected);
        }
      case 'reply':
        return;
    }
  }

  void _fail(Object error) {
    _state = TelemetrySourceState.error;
    _publishStatus(TelemetrySnapshotStatus.error);
  }

  void _publishStatus(TelemetrySnapshotStatus status) {
    final current = _latest;
    if (current == null) return;
    final marked = current.copyWith(status: status);
    _latest = marked;
    _controller.add(marked);
  }

  void _checkStale() {
    final current = _latest;
    if (current == null) return;
    if (_state != TelemetrySourceState.connected) return;
    if (_clock().difference(current.capturedAt) < staleAfter) return;
    final marked = current.copyWith(status: TelemetrySnapshotStatus.stale);
    _latest = marked;
    _state = TelemetrySourceState.stale;
    _controller.add(marked);
  }

  @override
  Future<void> stop() async {
    await _subscription?.cancel();
    _subscription = null;
    _staleTimer?.cancel();
    _staleTimer = null;
    _state = TelemetrySourceState.idle;
  }

  @override
  Future<void> dispose() async {
    await stop();
    await _controller.close();
  }
}
