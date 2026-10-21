import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// Read-only aRacer link driven by the Kotlin protocol bridge.
class ProtocolService extends ChangeNotifier {
  ProtocolService._();

  static final ProtocolService instance = ProtocolService._();

  static const MethodChannel _methods = MethodChannel('moto/protocol');
  static const EventChannel _events = EventChannel('moto/protocol/events');

  static const List<String> dashboardParameters = [
    'RPM',
    'TPS_Percent',
    'T_Eng',
    'Volt_Batt',
    'SA',
    'AFR_Target',
  ];

  StreamSubscription<dynamic>? _subscription;
  final _updates = StreamController<Map<String, Object?>>.broadcast();
  bool _started = false;

  String state = 'idle';
  String? message;
  String host = '192.168.88.88';
  int port = 6666;
  String ssid = '';
  String password = '';
  Map<String, double> values = const {};
  DateTime? updatedAt;
  int telemetryCount = 0;
  bool busy = false;

  bool get connected => state == 'connected';
  bool get hasData => updatedAt != null;
  Stream<Map<String, Object?>> get updates => _updates.stream;

  void start() {
    if (_started) return;
    _started = true;
    _subscription = _events.receiveBroadcastStream().listen(
      _onEvent,
      onError: (Object error) {
        state = 'error';
        message = error.toString();
        notifyListeners();
      },
    );
  }

  void _onEvent(dynamic event) {
    if (event is! Map) return;
    _updates.add(event.map((key, value) => MapEntry('$key', value)));
    switch (event['type']) {
      case 'status':
        state = (event['state'] as String?) ?? state;
        message = event['message'] as String?;
        if (state == 'closed' || state == 'error') {
          values = const {};
          updatedAt = null;
        }
      case 'telemetry':
        final raw = event['values'];
        if (raw is Map) {
          values = raw.map(
            (key, value) => MapEntry('$key', (value as num).toDouble()),
          );
          updatedAt = DateTime.now();
          telemetryCount++;
        }
      case 'reply':
        message = 'reply ${event['canId']}';
    }
    notifyListeners();
  }

  Future<bool> connect({
    String? newHost,
    int? newPort,
    String? newSsid,
    String? newPassword,
  }) async {
    host = newHost ?? host;
    port = newPort ?? port;
    ssid = newSsid ?? ssid;
    password = newPassword ?? password;
    busy = true;
    state = 'connecting';
    notifyListeners();
    try {
      final ok = await _methods.invokeMethod<bool>('connect', {
        'host': host,
        'port': port,
        if (ssid.isNotEmpty) 'ssid': ssid,
        if (password.isNotEmpty) 'password': password,
      });
      return ok ?? false;
    } on PlatformException catch (error) {
      state = 'error';
      message = error.message ?? error.code;
      return false;
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    try {
      await _methods.invokeMethod<void>('disconnect');
    } on PlatformException {
      state = 'error';
    }
    notifyListeners();
  }

  Future<int> subscribe([List<String> names = dashboardParameters]) async {
    try {
      final count = await _methods.invokeMethod<int>('subscribe', {
        'names': names,
      });
      return count ?? 0;
    } on PlatformException catch (error) {
      message = error.message ?? error.code;
      notifyListeners();
      return 0;
    }
  }

  Future<void> requestDeviceName() => _quiet('requestDeviceName');
  Future<void> requestIdentity([int sub = 0]) =>
      _quiet('requestIdentity', {'sub': sub});

  Future<void> _quiet(String method, [Map<String, Object?>? args]) async {
    try {
      await _methods.invokeMethod<dynamic>(method, args);
    } on PlatformException catch (error) {
      message = error.message ?? error.code;
      notifyListeners();
    }
  }

  double value(String name, [double fallback = 0]) => values[name] ?? fallback;

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }
}
