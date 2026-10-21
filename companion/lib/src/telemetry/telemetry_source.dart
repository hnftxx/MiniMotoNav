import 'telemetry_snapshot.dart';

enum TelemetrySourceState {
  idle,
  connecting,
  connected,
  stale,
  disconnected,
  error,
}

abstract class TelemetrySource {
  String get id;
  String get label;
  TelemetrySourceState get state;
  TelemetrySnapshot? get latest;
  Stream<TelemetrySnapshot> get snapshots;

  Future<void> start();
  Future<void> stop();
  Future<void> dispose();
}
