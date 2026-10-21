enum NavigationManeuver {
  none,
  straight,
  slightLeft,
  left,
  sharpLeft,
  slightRight,
  right,
  sharpRight,
  uTurn,
  roundabout,
  arrive,
}

/// Where the current guidance was produced. The source is app-level only; the
/// version-1 BLE navigation payload carries no source field.
enum NavigationSource { none, googleCompanion, osm }

class RoutePoint {
  const RoutePoint(this.latitude, this.longitude);

  final double latitude;
  final double longitude;
}

const Object _unset = Object();

class NavigationState {
  const NavigationState({
    this.maneuver = NavigationManeuver.none,
    this.distanceToManeuverM = 0,
    this.roadName = '',
    this.eta,
    this.remainingDistanceKm = 0,
    this.gpsAccuracyM = 0,
    this.routeValid = false,
    this.gpsFix = false,
    this.routeGeometry = const [],
    this.source = NavigationSource.none,
    this.updatedAt,
  });

  final NavigationManeuver maneuver;
  final double distanceToManeuverM;
  final String roadName;
  final DateTime? eta;
  final double remainingDistanceKm;
  final double gpsAccuracyM;
  final bool routeValid;
  final bool gpsFix;
  final List<RoutePoint> routeGeometry;
  final NavigationSource source;
  final DateTime? updatedAt;

  NavigationState copyWith({
    NavigationManeuver? maneuver,
    double? distanceToManeuverM,
    String? roadName,
    Object? eta = _unset,
    double? remainingDistanceKm,
    double? gpsAccuracyM,
    bool? routeValid,
    bool? gpsFix,
    List<RoutePoint>? routeGeometry,
    NavigationSource? source,
    Object? updatedAt = _unset,
  }) {
    return NavigationState(
      maneuver: maneuver ?? this.maneuver,
      distanceToManeuverM: distanceToManeuverM ?? this.distanceToManeuverM,
      roadName: roadName ?? this.roadName,
      eta: eta == _unset ? this.eta : eta as DateTime?,
      remainingDistanceKm: remainingDistanceKm ?? this.remainingDistanceKm,
      gpsAccuracyM: gpsAccuracyM ?? this.gpsAccuracyM,
      routeValid: routeValid ?? this.routeValid,
      gpsFix: gpsFix ?? this.gpsFix,
      routeGeometry: routeGeometry ?? this.routeGeometry,
      source: source ?? this.source,
      updatedAt: updatedAt == _unset
          ? this.updatedAt
          : updatedAt as DateTime?,
    );
  }
}
