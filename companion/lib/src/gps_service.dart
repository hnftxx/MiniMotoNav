import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:geolocator/geolocator.dart';

import 'navigation/navigation_service.dart';

class GpsPoint {
  const GpsPoint(this.latitude, this.longitude, this.heading, this.at);

  final double latitude;
  final double longitude;
  final double heading;
  final DateTime at;
}

/// Real device GPS. No route engine: this exposes the raw fix, heading,
/// speed and a bounded breadcrumb trail for the navigation screen.
class GpsService extends ChangeNotifier {
  GpsService._();

  static final GpsService instance = GpsService._();

  StreamSubscription<Position>? _subscription;
  bool _started = false;

  Position? position;
  String status = 'idle';
  bool permissionGranted = false;
  double tripMeters = 0;
  final List<GpsPoint> trail = [];

  bool get hasFix => position != null;
  double get speedKph => (position?.speed ?? 0) * 3.6;
  double get heading => position?.heading ?? 0;
  int get satellites => 0; // Android does not expose a satellite count here.

  Future<void> start() async {
    if (_started) return;
    _started = true;
    status = 'checking';
    notifyListeners();

    if (!await Geolocator.isLocationServiceEnabled()) {
      status = 'gps off';
      notifyListeners();
      return;
    }
    var permission = await Geolocator.checkPermission();
    if (permission == LocationPermission.denied) {
      permission = await Geolocator.requestPermission();
    }
    if (permission == LocationPermission.denied ||
        permission == LocationPermission.deniedForever) {
      status = 'permission denied';
      notifyListeners();
      return;
    }
    permissionGranted = true;
    status = 'searching';

    final last = await Geolocator.getLastKnownPosition();
    if (last != null) _accept(last);

    _subscription =
        Geolocator.getPositionStream(
          locationSettings: const LocationSettings(
            accuracy: LocationAccuracy.bestForNavigation,
            distanceFilter: 1,
          ),
        ).listen(
          _accept,
          onError: (Object error) {
            status = 'gps error';
            notifyListeners();
          },
        );
  }

  void _accept(Position next) {
    final previous = position;
    if (previous != null) {
      tripMeters += Geolocator.distanceBetween(
        previous.latitude,
        previous.longitude,
        next.latitude,
        next.longitude,
      );
    }
    position = next;
    status = next.accuracy <= 8 ? '3d fix' : 'weak fix';
    NavigationService.instance.updateFromGps(next);
    if (next.latitude != 0 || next.longitude != 0) {
      trail.add(
        GpsPoint(next.latitude, next.longitude, next.heading, next.timestamp),
      );
      if (trail.length > 400) trail.removeRange(0, trail.length - 400);
    }
    notifyListeners();
  }

  double get tripKm => tripMeters / 1000;

  double bearingTo(GpsPoint target) {
    final from = position;
    if (from == null) return 0;
    final lat1 = from.latitude * math.pi / 180;
    final lat2 = target.latitude * math.pi / 180;
    final dLon = (target.longitude - from.longitude) * math.pi / 180;
    final y = math.sin(dLon) * math.cos(lat2);
    final x =
        math.cos(lat1) * math.sin(lat2) -
        math.sin(lat1) * math.cos(lat2) * math.cos(dLon);
    return (math.atan2(y, x) * 180 / math.pi + 360) % 360;
  }

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }
}
