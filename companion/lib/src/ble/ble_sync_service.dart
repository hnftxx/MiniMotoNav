import 'dart:async';

import '../navigation/navigation_service.dart';
import '../telemetry/telemetry_service.dart';
import 'ble_service.dart';

class BleSyncService {
  BleSyncService._();

  static final BleSyncService instance = BleSyncService._();

  final _ble = BleService.instance;
  final _telemetry = TelemetryService.instance;
  final _navigation = NavigationService.instance;
  Timer? _telemetryTimer;
  Timer? _navigationTimer;
  bool _wasReady = false;
  bool _started = false;

  void start() {
    if (_started) return;
    _started = true;
    _ble.start();
    _ble.addListener(_onLink);
    _telemetry.addListener(_onTelemetry);
    _navigation.addListener(_onNavigation);
  }

  void _onLink() {
    final ready = _ble.ready;
    if (ready && !_wasReady) {
      _sendTelemetryNow();
      _sendNavigationNow();
    }
    _wasReady = ready;
  }

  void _onTelemetry() {
    if (!_ble.ready) return;
    if (_telemetryTimer?.isActive ?? false) return;
    _telemetryTimer = Timer(const Duration(milliseconds: 100), () {
      _sendTelemetryNow();
      _telemetryTimer = null;
    });
  }

  void _onNavigation() {
    if (!_ble.ready) return;
    if (_navigationTimer?.isActive ?? false) return;
    _navigationTimer = Timer(const Duration(milliseconds: 200), () {
      _sendNavigationNow();
      _navigationTimer = null;
    });
  }

  void _sendTelemetryNow() {
    final snapshot = _telemetry.latest;
    if (snapshot == null || !_ble.ready) return;
    _ble.sendTelemetry(snapshot);
  }

  void _sendNavigationNow() {
    if (!_ble.ready) return;
    _ble.sendNavigation(_navigation.state);
  }
}
