import 'package:flutter/foundation.dart';

import 'aracer_tcp_telemetry_source.dart';
import 'simulator_telemetry_source.dart';
import 'telemetry_snapshot.dart';
import 'telemetry_source.dart';
import 'telemetry_source_controller.dart';

class TelemetryService extends ChangeNotifier {
  TelemetryService._() {
    _ecu = AracerTcpTelemetrySource();
    _simulator = SimulatorTelemetrySource();
    _controller = TelemetrySourceController(_ecu);
    _controller.addListener(notifyListeners);
  }

  static final TelemetryService instance = TelemetryService._();

  late final AracerTcpTelemetrySource _ecu;
  late final SimulatorTelemetrySource _simulator;
  late final TelemetrySourceController _controller;
  bool _preview = false;

  TelemetrySnapshot? get latest => _controller.latest;

  TelemetrySourceController get controller => _controller;

  TelemetrySourceState get state => _controller.state;

  bool get preview => _preview;

  bool get hasData => _controller.hasData;

  bool get connected => _controller.state == TelemetrySourceState.connected;

  bool get stale => _controller.stale;

  int get telemetryCount => _controller.freshCount;

  Map<String, double> get values =>
      _controller.latest?.toDashboardValues() ?? const {};

  String get sourceLabel => _controller.source.label;

  Future<void> start() => _controller.start();

  Future<void> setPreview(bool preview) async {
    if (_preview == preview && _controller.state != TelemetrySourceState.idle) {
      return;
    }
    _preview = preview;
    await _controller.switchTo(preview ? _simulator : _ecu);
    notifyListeners();
  }

  @override
  void dispose() {
    _controller.dispose();
    _simulator.dispose();
    _ecu.dispose();
    super.dispose();
  }
}
