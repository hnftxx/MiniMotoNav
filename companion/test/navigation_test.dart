import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:moto_companion/src/navigation/navigation_service.dart';
import 'package:moto_companion/src/navigation/navigation_state.dart';
import 'package:moto_companion/src/navigation/notification_nav_parser.dart';
import 'package:moto_companion/src/navigation/osm_service.dart';

const _postedAt = 1750000000000; // 2025-06-15T14:26:40Z
DateTime get _posted => DateTime.fromMillisecondsSinceEpoch(_postedAt);

CompanionGuidance _parse({
  String? title,
  String? text,
  String? bigText,
  String? subText,
  DateTime? now,
}) {
  final guidance = NotificationNavParser.parse(
    {
      'key': '0|com.google.android.apps.maps|1|null|10',
      'package': 'com.google.android.apps.maps',
      'postTime': _postedAt,
      if (title != null) 'title': title,
      if (text != null) 'text': text,
      if (bigText != null) 'bigText': bigText,
      if (subText != null) 'subText': subText,
    },
    now: now,
  );
  return guidance!;
}

void main() {
  group('Google Maps notification parsing', () {
    test('turn instruction with distance and road name', () {
      final guidance = _parse(
        title: 'Turn right onto Main St',
        text: 'In 300 m',
      );
      expect(guidance.maneuver, NavigationManeuver.right);
      expect(guidance.distanceToManeuverM, closeTo(300, 0.001));
      expect(guidance.roadName, 'Main St');
      expect(guidance.arrived, isFalse);
      expect(guidance.postedAt, _posted);
    });

    test('kilometres, miles and feet convert to metres', () {
      expect(
        _parse(title: 'Turn left', text: 'In 0.5 km').distanceToManeuverM,
        closeTo(500, 0.001),
      );
      expect(
        _parse(title: 'Turn left', text: 'In 1.2 mi').distanceToManeuverM,
        closeTo(1931.2128, 0.01),
      );
      expect(
        _parse(title: 'Turn left', text: 'In 500 ft').distanceToManeuverM,
        closeTo(152.4, 0.001),
      );
    });

    test('roundabout beats generic left/right keywords', () {
      final guidance = _parse(
        title: 'At the roundabout, take the 2nd exit onto A1',
        text: 'In 120 m',
      );
      expect(guidance.maneuver, NavigationManeuver.roundabout);
      expect(guidance.roadName, 'A1');
    });

    test('slight and u-turn modifiers', () {
      expect(
        _parse(title: 'Keep left onto R101', text: 'In 80 m').maneuver,
        NavigationManeuver.slightLeft,
      );
      expect(
        _parse(title: 'Make a U-turn', text: 'In 40 m').maneuver,
        NavigationManeuver.uTurn,
      );
    });

    test('arrival clears distance and uses arrive maneuver', () {
      final guidance = _parse(
        title: 'You have arrived',
        text: 'Your destination is on the right',
      );
      expect(guidance.arrived, isTrue);
      expect(guidance.maneuver, NavigationManeuver.arrive);
      expect(guidance.distanceToManeuverM, 0);
    });

    test('ETA clock uses notification day and rolls past midnight', () {
      final evening = DateTime(2025, 6, 15, 23, 40);
      final guidance = NotificationNavParser.parse(
        {
          'key': 'k',
          'postTime': evening.millisecondsSinceEpoch,
          'title': 'Turn right onto Main St',
          'text': 'In 2 km',
          'bigText': 'Arrive 00:05',
        },
        now: evening,
      );
      expect(guidance!.eta, DateTime(2025, 6, 16, 0, 5));
    });

    test('minutes fallback adds to post time', () {
      final guidance = _parse(
        title: 'Turn right onto Main St',
        text: 'In 2 km · 18 min',
      );
      expect(guidance.eta, _posted.add(const Duration(minutes: 18)));
    });

    test('non-navigation notifications are ignored', () {
      expect(
        NotificationNavParser.parse({
          'key': 'k',
          'postTime': _postedAt,
          'title': 'Alice',
          'text': 'See you at 6',
        }),
        isNull,
      );
      expect(NotificationNavParser.parse(const {}), isNull);
    });
  });

  group('OSRM route parsing', () {
    final reply = <String, dynamic>{
      'code': 'Ok',
      'routes': [
        {
          'distance': 1000.0,
          'duration': 120.0,
          'geometry': {
            'type': 'LineString',
            'coordinates': [
              [11.0, 48.0],
              [11.01, 48.0],
              [11.01, 48.009],
            ],
          },
          'legs': [
            {
              'steps': [
                {
                  'distance': 400.0,
                  'name': 'Start St',
                  'maneuver': {
                    'type': 'depart',
                    'modifier': 'right',
                    'location': [11.0, 48.0],
                  },
                },
                {
                  'distance': 600.0,
                  'name': 'Main St',
                  'maneuver': {
                    'type': 'turn',
                    'modifier': 'slight left',
                    'location': [11.01, 48.0],
                  },
                },
              ],
            },
          ],
        },
      ],
    };

    test('steps accumulate distance and finish with arrive', () {
      final plan = OsmService.parseRoute(reply, destinationName: 'Home');
      expect(plan.destinationName, 'Home');
      expect(plan.points.length, 3);
      expect(plan.steps.length, 2);
      expect(plan.steps.first.maneuver, NavigationManeuver.right);
      expect(plan.steps.first.endDistanceM, closeTo(400, 0.001));
      expect(plan.steps.last.maneuver, NavigationManeuver.arrive);
      expect(plan.steps.last.roadName, 'Main St');
      expect(plan.steps.last.endDistanceM, closeTo(1000, 0.001));
      expect(plan.destination.latitude, closeTo(48.009, 1e-9));
    });

    test('progress reports off-route distance and next step', () {
      final plan = OsmService.parseRoute(reply);
      final onRoute = plan.progressAt(const LatLng(48.0, 11.005));
      expect(onRoute.offRouteM, lessThan(5));
      expect(onRoute.step, plan.steps.first);
      expect(onRoute.distanceToStepM, closeTo(400 - 370, 30));

      final offRoute = plan.progressAt(const LatLng(48.0, 11.02));
      expect(offRoute.offRouteM, greaterThan(500));
    });

    test('remaining distance and time shrink along the route', () {
      final plan = OsmService.parseRoute(reply);
      final progress = plan.progressAt(const LatLng(48.0, 11.005));
      expect(plan.remainingMeters(progress.alongM), lessThan(1000));
      expect(
        plan.remainingSeconds(progress.alongM),
        closeTo(120 * (1 - progress.alongM / 1000), 5),
      );
    });

    test('empty route and malformed geometry are rejected', () {
      expect(
        () => OsmService.parseRoute(const {'routes': []}),
        throwsA(isA<OsmException>()),
      );
      expect(
        () => OsmService.parseRoute({
          'routes': [
            {'distance': 1, 'duration': 1, 'geometry': {}},
          ],
        }),
        throwsA(isA<OsmException>()),
      );
    });

    test('OSRM maneuver strings map onto the display enum', () {
      expect(OsmService.mapManeuver('turn slight right'), NavigationManeuver.slightRight);
      expect(OsmService.mapManeuver('turn sharp left'), NavigationManeuver.sharpLeft);
      expect(OsmService.mapManeuver('roundabout rotary'), NavigationManeuver.roundabout);
      expect(OsmService.mapManeuver('arrive destination'), NavigationManeuver.arrive);
      expect(OsmService.mapManeuver('turn uturn'), NavigationManeuver.uTurn);
    });

    test('Nominatim results parse name and coordinates', () {
      final place = OsmService.parsePlace(const {
        'lat': '48.1374',
        'lon': '11.5755',
        'display_name': 'Marienplatz, Munich, Bavaria, Germany',
        'name': 'Marienplatz',
      });
      expect(place!.name, 'Marienplatz');
      expect(place.point.latitude, closeTo(48.1374, 1e-9));
      expect(place.point.longitude, closeTo(11.5755, 1e-9));
      expect(OsmService.parsePlace(const {'lat': 'nope'}), isNull);
    });
  });

  group('navigation guidance sources', () {
    setUp(NavigationService.instance.clearGuidance);

    test('source, eta and timestamp survive until explicitly cleared', () {
      final service = NavigationService.instance;
      final eta = DateTime(2025, 6, 15, 15);
      final updatedAt = DateTime(2025, 6, 15, 14, 30);
      service.setGuidance(
        maneuver: NavigationManeuver.left,
        distanceToManeuverM: 120,
        roadName: 'Main St',
        eta: eta,
        remainingDistanceKm: 3.4,
        routeValid: true,
        source: NavigationSource.googleCompanion,
        updatedAt: updatedAt,
      );
      expect(service.state.source, NavigationSource.googleCompanion);
      expect(service.state.eta, eta);
      expect(service.state.updatedAt, updatedAt);
      expect(service.state.routeValid, isTrue);

      service.markGuidanceStale();
      expect(service.state.routeValid, isFalse);
      expect(service.state.source, NavigationSource.googleCompanion);
      expect(service.state.eta, eta);

      service.clearGuidance();
      expect(service.state.source, NavigationSource.none);
      expect(service.state.eta, isNull);
      expect(service.state.updatedAt, isNull);
      expect(service.state.roadName, '');
    });

    test('OSM guidance replaces companion guidance', () {
      final service = NavigationService.instance;
      service.setGuidance(
        maneuver: NavigationManeuver.right,
        distanceToManeuverM: 50,
        roadName: 'Companion Rd',
        eta: null,
        remainingDistanceKm: 1,
        routeValid: true,
        source: NavigationSource.googleCompanion,
      );
      service.setGuidance(
        maneuver: NavigationManeuver.straight,
        distanceToManeuverM: 500,
        roadName: 'Osm Rd',
        eta: null,
        remainingDistanceKm: 9,
        routeValid: true,
        source: NavigationSource.osm,
        routeGeometry: const [RoutePoint(48, 11)],
      );
      expect(service.state.source, NavigationSource.osm);
      expect(service.state.roadName, 'Osm Rd');
      expect(service.state.routeGeometry.length, 1);
    });

    test('clearing an already empty state is stable', () {
      final service = NavigationService.instance;
      service.clearGuidance();
      service.clearGuidance();
      expect(service.state.source, NavigationSource.none);
      expect(service.state.maneuver, NavigationManeuver.none);
    });
  });
}
