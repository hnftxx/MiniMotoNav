import 'dart:convert';
import 'dart:math' as math;

import 'package:http/http.dart' as http;
import 'package:latlong2/latlong.dart';

import 'navigation_state.dart';

class OsmException implements Exception {
  const OsmException(this.message);

  final String message;

  @override
  String toString() => message;
}

class PlaceResult {
  const PlaceResult({
    required this.name,
    required this.address,
    required this.point,
  });

  final String name;
  final String address;
  final LatLng point;
}

class RouteStep {
  const RouteStep({
    required this.maneuver,
    required this.roadName,
    required this.distanceM,
    required this.endDistanceM,
    required this.point,
  });

  final NavigationManeuver maneuver;
  final String roadName;
  final double distanceM;
  final double endDistanceM;
  final LatLng point;
}

class RoutePlan {
  RoutePlan({
    required this.points,
    required this.steps,
    required this.distanceM,
    required this.durationS,
    required this.destinationName,
  }) : cumulative = _cumulative(points);

  final List<LatLng> points;
  final List<RouteStep> steps;
  final double distanceM;
  final double durationS;
  final String destinationName;
  final List<double> cumulative;

  LatLng get origin => points.first;
  LatLng get destination => points.last;

  RouteProgress progressAt(LatLng position) {
    if (points.length < 2) {
      return RouteProgress(
        alongM: 0,
        offRouteM: distanceMeters(position, points.first),
        distanceToStepM: 0,
        step: steps.isEmpty ? null : steps.first,
      );
    }
    var best = double.infinity;
    var bestAlong = 0.0;
    final cosLat = math.cos(position.latitude * math.pi / 180);
    for (var i = 0; i < points.length - 1; i++) {
      final a = points[i];
      final b = points[i + 1];
      final ax = (a.longitude - position.longitude) * 111320 * cosLat;
      final ay = (a.latitude - position.latitude) * 111320;
      final bx = (b.longitude - position.longitude) * 111320 * cosLat;
      final by = (b.latitude - position.latitude) * 111320;
      final dx = bx - ax;
      final dy = by - ay;
      final lengthSq = dx * dx + dy * dy;
      var t = 0.0;
      if (lengthSq > 0.01) {
        t = (-(ax * dx + ay * dy) / lengthSq).clamp(0.0, 1.0);
      }
      final px = ax + dx * t;
      final py = ay + dy * t;
      final off = math.sqrt(px * px + py * py);
      if (off < best) {
        best = off;
        bestAlong = cumulative[i] + (cumulative[i + 1] - cumulative[i]) * t;
      }
    }
    final step = _stepAt(bestAlong);
    return RouteProgress(
      alongM: bestAlong,
      offRouteM: best,
      distanceToStepM: step == null
          ? 0
          : (step.endDistanceM - bestAlong).clamp(0.0, double.infinity),
      step: step,
    );
  }

  RouteStep? _stepAt(double alongM) {
    for (final step in steps) {
      if (step.endDistanceM > alongM) return step;
    }
    return steps.isEmpty ? null : steps.last;
  }

  double remainingMeters(double alongM) =>
      (distanceM - alongM).clamp(0.0, distanceM);

  double remainingSeconds(double alongM) {
    if (distanceM <= 0) return 0;
    return durationS * (remainingMeters(alongM) / distanceM);
  }

  static List<double> _cumulative(List<LatLng> points) {
    final values = List<double>.filled(points.length, 0);
    for (var i = 1; i < points.length; i++) {
      values[i] = values[i - 1] + distanceMeters(points[i - 1], points[i]);
    }
    return values;
  }
}

class RouteProgress {
  const RouteProgress({
    required this.alongM,
    required this.offRouteM,
    required this.distanceToStepM,
    required this.step,
  });

  final double alongM;
  final double offRouteM;
  final double distanceToStepM;
  final RouteStep? step;
}

class OsmService {
  OsmService({http.Client? client}) : _client = client ?? http.Client();

  static const _userAgent =
      'GODSPEED/1.0 (motorcycle dashboard companion; +https://github.com/hnftxx)';
  static const _searchHost = 'nominatim.openstreetmap.org';
  static const _routerHost = 'router.project-osrm.org';

  final http.Client _client;

  Future<List<PlaceResult>> search(String query, {LatLng? near}) async {
    final trimmed = query.trim();
    if (trimmed.isEmpty) return const [];
    final uri = Uri.https(_searchHost, '/search', {
      'format': 'jsonv2',
      'q': trimmed,
      'limit': '8',
      'addressdetails': '0',
      if (near != null) 'viewbox': _viewbox(near),
      if (near != null) 'bounded': '0',
    });
    final response = await _get(uri);
    final decoded = jsonDecode(response.body);
    if (decoded is! List) {
      throw const OsmException('Place search returned an unexpected reply');
    }
    return decoded
        .whereType<Map<String, dynamic>>()
        .map(parsePlace)
        .whereType<PlaceResult>()
        .toList(growable: false);
  }

  Future<RoutePlan> route({
    required LatLng from,
    required LatLng to,
    String destinationName = 'Destination',
  }) async {
    final uri = Uri.https(
      _routerHost,
      '/route/v1/driving/'
      '${from.longitude.toStringAsFixed(6)},${from.latitude.toStringAsFixed(6)};'
      '${to.longitude.toStringAsFixed(6)},${to.latitude.toStringAsFixed(6)}',
      {
        'overview': 'full',
        'geometries': 'geojson',
        'steps': 'true',
        'alternatives': 'false',
      },
    );
    final response = await _get(uri);
    final decoded = jsonDecode(response.body);
    if (decoded is! Map<String, dynamic>) {
      throw const OsmException('Routing returned an unexpected reply');
    }
    return parseRoute(decoded, destinationName: destinationName);
  }

  Future<http.Response> _get(Uri uri) async {
    final response = await _client
        .get(
          uri,
          headers: const {
            'User-Agent': _userAgent,
            'Accept': 'application/json',
          },
        )
        .timeout(const Duration(seconds: 20));
    if (response.statusCode != 200) {
      throw OsmException('Request failed (HTTP ${response.statusCode})');
    }
    return response;
  }

  void close() => _client.close();

  static String _viewbox(LatLng near) {
    const span = 1.0;
    final left = near.longitude - span;
    final right = near.longitude + span;
    final top = near.latitude + span;
    final bottom = near.latitude - span;
    return '$left,$top,$right,$bottom';
  }

  static PlaceResult? parsePlace(Map<String, dynamic> json) {
    final lat = double.tryParse('${json['lat']}');
    final lon = double.tryParse('${json['lon']}');
    if (lat == null || lon == null) return null;
    final display = (json['display_name'] as String?)?.trim() ?? '';
    final name = (json['name'] as String?)?.trim();
    final first = display.split(',').first.trim();
    return PlaceResult(
      name: (name == null || name.isEmpty) ? first : name,
      address: display,
      point: LatLng(lat, lon),
    );
  }

  static RoutePlan parseRoute(
    Map<String, dynamic> json, {
    String destinationName = 'Destination',
  }) {
    final routes = json['routes'];
    if (routes is! List || routes.isEmpty) {
      throw const OsmException('No route found');
    }
    final route = routes.first;
    if (route is! Map<String, dynamic>) {
      throw const OsmException('Route reply is malformed');
    }
    final distance = (route['distance'] as num?)?.toDouble();
    final duration = (route['duration'] as num?)?.toDouble();
    final geometry = route['geometry'];
    if (distance == null ||
        duration == null ||
        geometry is! Map<String, dynamic> ||
        geometry['coordinates'] is! List) {
      throw const OsmException('Route reply is missing geometry');
    }
    final points = <LatLng>[];
    for (final coordinate in geometry['coordinates'] as List) {
      if (coordinate is! List || coordinate.length < 2) continue;
      final lon = (coordinate[0] as num).toDouble();
      final lat = (coordinate[1] as num).toDouble();
      points.add(LatLng(lat, lon));
    }
    if (points.length < 2) {
      throw const OsmException('Route geometry is too short');
    }
    final steps = <RouteStep>[];
    final legs = route['legs'];
    if (legs is List) {
      for (final leg in legs.whereType<Map<String, dynamic>>()) {
        final rawSteps = leg['steps'];
        if (rawSteps is! List) continue;
        for (final raw in rawSteps.whereType<Map<String, dynamic>>()) {
          final maneuver = raw['maneuver'];
          if (maneuver is! Map<String, dynamic>) continue;
          final stepDistance = (raw['distance'] as num?)?.toDouble() ?? 0;
          final startDistance = steps.isEmpty ? 0.0 : steps.last.endDistanceM;
          final end = (startDistance + stepDistance).clamp(0.0, distance);
          final location = maneuver['location'];
          var point = points.first;
          if (location is List && location.length >= 2) {
            point = LatLng(
              (location[1] as num).toDouble(),
              (location[0] as num).toDouble(),
            );
          }
          steps.add(
            RouteStep(
              maneuver: mapManeuver(
                '${maneuver['type']} ${maneuver['modifier'] ?? ''}',
              ),
              roadName: (raw['name'] as String?)?.trim() ?? '',
              distanceM: stepDistance,
              endDistanceM: end,
              point: point,
            ),
          );
        }
      }
    }
    if (steps.isEmpty) {
      steps.add(
        RouteStep(
          maneuver: NavigationManeuver.straight,
          roadName: '',
          distanceM: distance,
          endDistanceM: distance,
          point: points.last,
        ),
      );
    } else {
      steps.last = RouteStep(
        maneuver: NavigationManeuver.arrive,
        roadName: steps.last.roadName,
        distanceM: steps.last.distanceM,
        endDistanceM: distance,
        point: steps.last.point,
      );
    }
    return RoutePlan(
      points: points,
      steps: steps,
      distanceM: distance,
      durationS: duration,
      destinationName: destinationName,
    );
  }

  static NavigationManeuver mapManeuver(String text) {
    final value = text.toLowerCase();
    if (value.contains('arrive') || value.contains('destination')) {
      return NavigationManeuver.arrive;
    }
    if (value.contains('roundabout') || value.contains('rotary')) {
      return NavigationManeuver.roundabout;
    }
    if (value.contains('uturn') || value.contains('u-turn')) {
      return NavigationManeuver.uTurn;
    }
    if (value.contains('sharp left')) return NavigationManeuver.sharpLeft;
    if (value.contains('sharp right')) return NavigationManeuver.sharpRight;
    if (value.contains('slight left')) return NavigationManeuver.slightLeft;
    if (value.contains('slight right')) return NavigationManeuver.slightRight;
    if (value.contains('left')) return NavigationManeuver.left;
    if (value.contains('right')) return NavigationManeuver.right;
    return NavigationManeuver.straight;
  }
}

double distanceMeters(LatLng a, LatLng b) {
  const earth = 6371000.0;
  final dLat = (b.latitude - a.latitude) * math.pi / 180;
  final dLon = (b.longitude - a.longitude) * math.pi / 180;
  final lat1 = a.latitude * math.pi / 180;
  final lat2 = b.latitude * math.pi / 180;
  final h =
      math.sin(dLat / 2) * math.sin(dLat / 2) +
      math.cos(lat1) * math.cos(lat2) * math.sin(dLon / 2) * math.sin(dLon / 2);
  return 2 * earth * math.asin(math.min(1, math.sqrt(h)));
}
