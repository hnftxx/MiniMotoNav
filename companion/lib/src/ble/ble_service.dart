import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import '../navigation/navigation_state.dart';
import '../telemetry/telemetry_snapshot.dart';
import 'ble_protocol.dart';

class BleDevice {
  const BleDevice({
    required this.address,
    required this.name,
    required this.rssi,
  });

  final String address;
  final String name;
  final int rssi;
}

class BleService extends ChangeNotifier {
  BleService._();

  static final BleService instance = BleService._();

  static const _channel = MethodChannel('moto/ble');
  static const _events = EventChannel('moto/ble/events');

  StreamSubscription<dynamic>? _subscription;
  final Map<String, BleDevice> _devices = {};
  bool _started = false;
  bool _scanning = false;
  String _state = 'idle';
  String? _message;
  String? _address;
  String? _name;
  int _mtu = 23;
  int _sequence = 0;
  int _dropped = 0;

  bool get started => _started;
  bool get scanning => _scanning;
  String get state => _state;
  String? get message => _message;
  String? get address => _address;
  String? get name => _name;
  int get mtu => _mtu;
  int get dropped => _dropped;
  bool get connected => _state == 'connected' || _state == 'ready';
  bool get ready => _state == 'ready';
  int get maxWriteLength => (mtu - 3).clamp(20, 512).toInt();

  List<BleDevice> get devices {
    final list = _devices.values.toList();
    list.sort((a, b) => b.rssi.compareTo(a.rssi));
    return list;
  }

  Future<void> start() async {
    if (_started) return;
    _started = true;
    _subscription = _events.receiveBroadcastStream().listen(
      _onEvent,
      onError: (Object error) {
        _setState('error', '$error');
      },
    );
  }

  Future<bool> scan() async {
    await start();
    try {
      final started = await _channel.invokeMethod<bool>('scan') ?? false;
      if (!started && _state == 'idle') {
        _setState('unsupported', 'Bluetooth scan unavailable');
      }
      return started;
    } on PlatformException catch (error) {
      _setState('error', error.message ?? error.code);
      return false;
    } on MissingPluginException {
      _setState('unsupported', 'BLE bridge is unavailable on this platform');
      return false;
    }
  }

  Future<void> stopScan() async {
    try {
      await _channel.invokeMethod<void>('stopScan');
    } on PlatformException {
      // The next state event carries the failure.
    } on MissingPluginException {
      // Desktop and test hosts have no BLE bridge.
    }
    _scanning = false;
    notifyListeners();
  }

  Future<bool> connect(BleDevice device) async {
    await start();
    try {
      final ok =
          await _channel.invokeMethod<bool>('connect', {
            'address': device.address,
          }) ??
          false;
      if (ok) {
        _address = device.address;
        _name = device.name;
        notifyListeners();
      }
      return ok;
    } on PlatformException catch (error) {
      _setState('error', error.message ?? error.code);
      return false;
    } on MissingPluginException {
      _setState('unsupported', 'BLE bridge is unavailable on this platform');
      return false;
    }
  }

  Future<void> disconnect() async {
    try {
      await _channel.invokeMethod<void>('disconnect');
    } on PlatformException {
      // State events report the outcome.
    } on MissingPluginException {
      // No bridge to disconnect.
    }
    _address = null;
    _name = null;
    _setState('idle');
  }

  Future<void> requestPermissions() async {
    try {
      await _channel.invokeMethod<void>('requestPermissions');
    } on PlatformException {
      _setState('permission', 'Bluetooth permission was not granted');
    } on MissingPluginException {
      _setState('unsupported', 'BLE bridge is unavailable on this platform');
    }
  }

  int nextSequence() {
    _sequence = (_sequence + 1) & 0xFF;
    return _sequence;
  }

  Future<bool> sendTelemetry(TelemetrySnapshot snapshot) {
    final now = DateTime.now();
    final age = now.difference(snapshot.capturedAt).inMilliseconds;
    return _send(
      BleMessage(
        type: BleProtocol.typeTelemetry,
        flags: BleProtocol.flagValid,
        sequence: nextSequence(),
        timestampSeconds: now.millisecondsSinceEpoch ~/ 1000,
        dataAgeMs: age.clamp(0, 65535).toInt(),
        payload: BleProtocol.encodeTelemetry(snapshot),
      ),
    );
  }

  Future<bool> sendNavigation(NavigationState navigation) {
    final now = DateTime.now();
    return _send(
      BleMessage(
        type: BleProtocol.typeNavigation,
        flags: BleProtocol.flagValid,
        sequence: nextSequence(),
        timestampSeconds: now.millisecondsSinceEpoch ~/ 1000,
        dataAgeMs: 0,
        payload: BleProtocol.encodeNavigation(navigation),
      ),
    );
  }

  Future<bool> sendControl(int command) {
    return _send(
      BleMessage(
        type: BleProtocol.typeControl,
        flags: BleProtocol.flagValid,
        sequence: nextSequence(),
        timestampSeconds: DateTime.now().millisecondsSinceEpoch ~/ 1000,
        dataAgeMs: 0,
        payload: BleProtocol.encodeControl(command),
      ),
    );
  }

  Future<bool> _send(BleMessage message) async {
    if (!ready) return false;
    final fragments = BleProtocol.fragment(
      message,
      maxWriteLength: maxWriteLength,
    );
    final kind = switch (message.type) {
      BleProtocol.typeTelemetry => 'telemetry',
      BleProtocol.typeNavigation => 'navigation',
      _ => 'control',
    };
    for (final fragment in fragments) {
      try {
        final ok =
            await _channel.invokeMethod<bool>('send', {
              'kind': kind,
              'bytes': fragment,
            }) ??
            false;
        if (!ok) return false;
      } on PlatformException catch (error) {
        _setState('error', error.message ?? error.code);
        return false;
      } on MissingPluginException {
        _setState('unsupported', 'BLE bridge is unavailable on this platform');
        return false;
      }
    }
    return true;
  }

  void _onEvent(dynamic event) {
    if (event is! Map) return;
    final map = event.cast<Object?, Object?>();
    switch (map['event']) {
      case 'state':
        final state = map['state'] as String? ?? 'idle';
        final message = map['message'] as String?;
        _setState(state, message);
      case 'device':
        final address = map['address'] as String?;
        if (address == null) return;
        _devices[address] = BleDevice(
          address: address,
          name: (map['name'] as String?) ?? 'Unnamed display',
          rssi: (map['rssi'] as num?)?.toInt() ?? -127,
        );
        notifyListeners();
      case 'mtu':
        _mtu = (map['mtu'] as num?)?.toInt() ?? _mtu;
        notifyListeners();
      case 'dropped':
        _dropped = (map['total'] as num?)?.toInt() ?? _dropped;
        notifyListeners();
      case 'status':
        _message = 'display status ${map['message'] ?? ''}';
        notifyListeners();
    }
  }

  void _setState(String state, [String? message]) {
    _state = state;
    _message = message;
    _scanning = state == 'scanning';
    if (state == 'idle' || state == 'disconnected') {
      _address = null;
      _name = null;
    }
    notifyListeners();
  }

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }
}
