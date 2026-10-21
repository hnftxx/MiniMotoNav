import 'package:flutter/foundation.dart';
import 'package:geolocator/geolocator.dart';

import 'navigation_state.dart';

class NavigationService extends ChangeNotifier {
  NavigationService._();

  static final NavigationService instance = NavigationService._();

  NavigationState _state = const NavigationState();

  NavigationState get state => _state;

  void updateFromGps(Position? position) {
    if (position == null) {
      final next = _state.copyWith(
        gpsFix: false,
        gpsAccuracyM: 0,
        routeValid: false,
      );
      if (next == _state) return;
      _state = next;
      notifyListeners();
      return;
    }
    final next = _state.copyWith(gpsFix: true, gpsAccuracyM: position.accuracy);
    _state = next;
    notifyListeners();
  }

  void setGuidance({
    required NavigationManeuver maneuver,
    required double distanceToManeuverM,
    required String roadName,
    required DateTime? eta,
    required double remainingDistanceKm,
    required bool routeValid,
    List<RoutePoint> routeGeometry = const [],
    NavigationSource source = NavigationSource.none,
    DateTime? updatedAt,
  }) {
    _state = _state.copyWith(
      maneuver: maneuver,
      distanceToManeuverM: distanceToManeuverM,
      roadName: roadName,
      eta: eta,
      remainingDistanceKm: remainingDistanceKm,
      routeValid: routeValid,
      routeGeometry: routeGeometry,
      source: source,
      updatedAt: updatedAt,
    );
    notifyListeners();
  }

  void markGuidanceStale() {
    if (!_state.routeValid) return;
    _state = _state.copyWith(routeValid: false);
    notifyListeners();
  }

  void clearGuidance() {
    _state = _state.copyWith(
      maneuver: NavigationManeuver.none,
      distanceToManeuverM: 0,
      roadName: '',
      eta: null,
      remainingDistanceKm: 0,
      routeValid: false,
      routeGeometry: const [],
      source: NavigationSource.none,
      updatedAt: null,
    );
    notifyListeners();
  }
}
