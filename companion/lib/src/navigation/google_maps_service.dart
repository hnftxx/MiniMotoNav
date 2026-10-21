import 'dart:convert';
import 'dart:io';

class PlaceSuggestion {
  const PlaceSuggestion({
    required this.id,
    required this.name,
    required this.address,
    required this.latitude,
    required this.longitude,
  });

  final String id;
  final String name;
  final String address;
  final double latitude;
  final double longitude;
}

class RouteOption {
  const RouteOption({
    required this.token,
    required this.distanceMeters,
    required this.durationSeconds,
  });

  final String token;
  final int distanceMeters;
  final int durationSeconds;

  String get distanceLabel => distanceMeters >= 1000
      ? '${(distanceMeters / 1000).toStringAsFixed(1)} km'
      : '$distanceMeters m';

  String get durationLabel {
    final minutes = (durationSeconds / 60).round();
    if (minutes < 60) return '$minutes min';
    final hours = minutes ~/ 60;
    final rest = minutes % 60;
    return '${hours}h ${rest}m';
  }
}

/// Google Maps Platform HTTP calls: Places Text Search and Routes API route
/// tokens. The key travels only in the request header; it is never logged.
class GoogleMapsService {
  GoogleMapsService(this.apiKey) : assert(apiKey.isNotEmpty);

  final String apiKey;

  Future<List<PlaceSuggestion>> searchPlaces(
    String query, {
    double? nearLatitude,
    double? nearLongitude,
  }) async {
    final body = <String, Object?>{
      'textQuery': query,
      'maxResultCount': 8,
    };
    if (nearLatitude != null && nearLongitude != null) {
      body['locationBias'] = {
        'circle': {
          'center': {
            'latitude': nearLatitude,
            'longitude': nearLongitude,
          },
          'radius': 30000.0,
        },
      };
    }
    final json = await _post(
      Uri.parse('https://places.googleapis.com/v1/places:searchText'),
      body,
      const [
        'places.id',
        'places.displayName',
        'places.formattedAddress',
        'places.location',
      ],
    );
    final places = (json['places'] as List?) ?? const [];
    return places
        .whereType<Map>()
        .map((place) {
          final map = place.cast<String, Object?>();
          final location = (map['location'] as Map?)?.cast<String, Object?>();
          final displayName = (map['displayName'] as Map?)
              ?.cast<String, Object?>();
          final lat = (location?['latitude'] as num?)?.toDouble();
          final lng = (location?['longitude'] as num?)?.toDouble();
          if (lat == null || lng == null) return null;
          return PlaceSuggestion(
            id: (map['id'] as String?) ?? '',
            name: (displayName?['text'] as String?) ?? 'Destination',
            address: (map['formattedAddress'] as String?) ?? '',
            latitude: lat,
            longitude: lng,
          );
        })
        .whereType<PlaceSuggestion>()
        .toList(growable: false);
  }

  Future<List<RouteOption>> routeOptions({
    required double originLatitude,
    required double originLongitude,
    required double destinationLatitude,
    required double destinationLongitude,
  }) async {
    final json = await _post(
      Uri.parse(
        'https://routes.googleapis.com/directions/v2:computeRoutes',
      ),
      {
        'origin': {
          'location': {
            'latLng': {
              'latitude': originLatitude,
              'longitude': originLongitude,
            },
          },
        },
        'destination': {
          'location': {
            'latLng': {
              'latitude': destinationLatitude,
              'longitude': destinationLongitude,
            },
          },
        },
        'travelMode': 'TWO_WHEELER',
        'routingPreference': 'TRAFFIC_AWARE',
        'computeAlternativeRoutes': true,
        'polylineQuality': 'OVERVIEW',
      },
      const [
        'routes.routeToken',
        'routes.distanceMeters',
        'routes.duration',
      ],
    );
    final routes = (json['routes'] as List?) ?? const [];
    return routes
        .whereType<Map>()
        .map((route) {
          final map = route.cast<String, Object?>();
          final token = (map['routeToken'] as String?) ?? '';
          if (token.isEmpty) return null;
          return RouteOption(
            token: token,
            distanceMeters: (map['distanceMeters'] as num?)?.toInt() ?? 0,
            durationSeconds: _seconds(map['duration'] as String?),
          );
        })
        .whereType<RouteOption>()
        .toList(growable: false);
  }

  Future<Map<String, Object?>> _post(
    Uri uri,
    Map<String, Object?> body,
    List<String> fieldMask,
  ) async {
    final client = HttpClient()..connectionTimeout = const Duration(seconds: 15);
    try {
      final request = await client.postUrl(uri);
      request.headers
        ..set(HttpHeaders.contentTypeHeader, 'application/json')
        ..set('X-Goog-Api-Key', apiKey)
        ..set('X-Goog-FieldMask', fieldMask.join(','));
      request.add(utf8.encode(jsonEncode(body)));
      final response = await request.close().timeout(const Duration(seconds: 20));
      final text = await response.transform(utf8.decoder).join();
      if (response.statusCode < 200 || response.statusCode >= 300) {
        throw GoogleMapsException(
          'Maps request failed (${response.statusCode})',
        );
      }
      final decoded = jsonDecode(text);
      if (decoded is! Map) {
        throw const GoogleMapsException('Maps response was not an object');
      }
      return decoded.cast<String, Object?>();
    } on SocketException {
      throw const GoogleMapsException('Network unavailable');
    } finally {
      client.close(force: true);
    }
  }

  int _seconds(String? duration) {
    if (duration == null || !duration.endsWith('s')) return 0;
    return double.tryParse(duration.substring(0, duration.length - 1))?.round() ??
        0;
  }
}

class GoogleMapsException implements Exception {
  const GoogleMapsException(this.message);

  final String message;

  @override
  String toString() => message;
}
