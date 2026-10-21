import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'navigation_service.dart';
import 'navigation_state.dart';
import 'notification_nav_parser.dart';

/// Bridges the Android notification listener into [NavigationService].
///
/// Only Google Maps notifications are forwarded by the platform side. Guidance
/// is marked stale after [staleAfter] and never synthesizes an ETA.
class MapsNotificationService extends ChangeNotifier {
  MapsNotificationService._();

  static final MapsNotificationService instance = MapsNotificationService._();

  static const _channel = EventChannel('moto/maps');
  static const _methods = MethodChannel('moto/maps/control');
  static const staleAfter = Duration(seconds: 30);

  StreamSubscription<dynamic>? _subscription;
  Timer? _watchdog;
  CompanionGuidance? _latest;
  DateTime? _receivedAt;
  bool _enabled = false;
  bool _listening = false;
  String? _lastError;

  CompanionGuidance? get latest => _latest;
  bool get enabled => _enabled;
  bool get stale {
    final at = _receivedAt;
    if (at == null) return false;
    return DateTime.now().difference(at) > staleAfter;
  }

  String? get lastError => _lastError;

  Future<void> start() async {
    if (_listening) return;
    _listening = true;
    _subscription = _channel.receiveBroadcastStream().listen(
      _onEvent,
      onError: (Object error) {
        _lastError = error.toString();
        notifyListeners();
      },
    );
    _watchdog = Timer.periodic(const Duration(seconds: 5), (_) {
      if (_latest == null) return;
      final previous = stale;
      notifyListeners();
      if (stale &&
          !previous &&
          NavigationService.instance.state.source ==
              NavigationSource.googleCompanion) {
        NavigationService.instance.markGuidanceStale();
      }
    });
    await refreshStatus();
  }

  Future<void> refreshStatus() async {
    try {
      final status = await _methods.invokeMapMethod<String, dynamic>('status');
      final enabled = status?['enabled'] == true;
      if (enabled != _enabled) {
        _enabled = enabled;
        notifyListeners();
      }
    } on MissingPluginException {
      _enabled = false;
    } on PlatformException catch (error) {
      _lastError = error.message ?? error.code;
    }
  }

  Future<void> openSettings() async {
    try {
      await _methods.invokeMethod<void>('openSettings');
    } on MissingPluginException {
      // Desktop/test hosts have no notification listener.
    } on PlatformException catch (error) {
      _lastError = error.message ?? error.code;
    }
  }

  void _onEvent(dynamic event) {
    if (event is! Map) return;
    final map = event.cast<Object?, Object?>();
    final kind = map['event'];
    if (kind == 'connected') {
      refreshStatus();
      return;
    }
    if (kind == 'removed') {
      final key = map['key'];
      if (key is String && _latest?.key == key && key.isNotEmpty) {
        _latest = null;
        _receivedAt = null;
        if (NavigationService.instance.state.source ==
            NavigationSource.googleCompanion) {
          NavigationService.instance.clearGuidance();
        }
        notifyListeners();
      }
      return;
    }
    if (kind != 'posted') return;
    final notification = map['notification'];
    if (notification is! Map) return;
    final guidance = NotificationNavParser.parse(
      notification.cast<Object?, Object?>(),
    );
    if (guidance == null) return;
    _latest = guidance;
    _receivedAt = DateTime.now();
    _publish(guidance);
    notifyListeners();
  }

  void _publish(CompanionGuidance guidance) {
    NavigationService.instance.setGuidance(
      maneuver: guidance.maneuver,
      distanceToManeuverM: guidance.distanceToManeuverM,
      roadName: guidance.roadName,
      eta: guidance.eta,
      remainingDistanceKm: 0,
      routeValid: !guidance.arrived || guidance.maneuver ==
          NavigationManeuver.arrive,
      source: NavigationSource.googleCompanion,
      updatedAt: guidance.postedAt,
    );
  }

  @override
  void dispose() {
    _watchdog?.cancel();
    _subscription?.cancel();
    super.dispose();
  }
}
