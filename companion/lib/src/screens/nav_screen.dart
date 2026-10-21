import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import '../app/theme.dart';
import '../gps_service.dart';
import '../navigation/maps_notification_service.dart';
import '../navigation/navigation_service.dart';
import '../navigation/navigation_state.dart';
import '../navigation/osm_service.dart';

class NavScreen extends StatefulWidget {
  const NavScreen({super.key});

  @override
  State<NavScreen> createState() => _NavScreenState();
}

class _NavScreenState extends State<NavScreen> with WidgetsBindingObserver {
  static const _offRouteLimitM = 60.0;
  static const _offRouteHold = Duration(seconds: 8);
  static const _rerouteCooldown = Duration(seconds: 30);

  final _map = MapController();
  final _osm = OsmService();
  final _gps = GpsService.instance;
  final _navigation = NavigationService.instance;
  final _maps = MapsNotificationService.instance;

  RoutePlan? _plan;
  PlaceResult? _destination;
  LatLng? _pendingDestination;
  String? _status;
  String? _error;
  bool _busy = false;
  bool _night = false;
  bool _follow = true;
  bool _arrived = false;
  bool _rerouting = false;
  DateTime? _offRouteSince;
  DateTime? _lastReroute;

  bool get _android =>
      !kIsWeb && defaultTargetPlatform == TargetPlatform.android;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _gps.addListener(_onGps);
    _maps.addListener(_onMaps);
    _gps.start();
    if (_android) unawaited(_maps.start());
    final fix = _gps.position;
    if (fix != null) {
      _map.move(LatLng(fix.latitude, fix.longitude), 16);
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _gps.removeListener(_onGps);
    _maps.removeListener(_onMaps);
    _osm.close();
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed && _android) {
      unawaited(_maps.refreshStatus());
    }
  }

  void _onMaps() {
    if (mounted) setState(() {});
  }

  void _onGps() {
    if (!mounted) return;
    final position = _gps.position;
    if (position == null) {
      setState(() {});
      return;
    }
    final here = LatLng(position.latitude, position.longitude);
    if (_follow) {
      _map.move(here, _map.camera.zoom.clamp(12, 18));
    }
    final pending = _pendingDestination;
    if (pending != null && !_busy) {
      _pendingDestination = null;
      unawaited(
        _routeTo(
          PlaceResult(
            name: _destination?.name ?? 'Destination',
            address: _destination?.address ?? '',
            point: pending,
          ),
        ),
      );
    }
    final plan = _plan;
    if (plan != null && !_arrived) {
      final progress = plan.progressAt(here);
      _publish(progress);
      _trackOffRoute(progress, here);
    }
    setState(() {});
  }

  void _publish(RouteProgress progress) {
    final companion = _maps.latest;
    if (companion != null &&
        !_maps.stale &&
        _navigation.state.source == NavigationSource.googleCompanion) {
      return;
    }
    final plan = _plan;
    if (plan == null) return;
    final step = progress.step;
    final remaining = plan.remainingMeters(progress.alongM);
    final seconds = plan.remainingSeconds(progress.alongM);
    if (remaining <= 20) {
      _arrived = true;
      _navigation.setGuidance(
        maneuver: NavigationManeuver.arrive,
        distanceToManeuverM: 0,
        roadName: step?.roadName ?? plan.destinationName,
        eta: DateTime.now(),
        remainingDistanceKm: 0,
        routeValid: true,
        source: NavigationSource.osm,
        updatedAt: DateTime.now(),
      );
      return;
    }
    _navigation.setGuidance(
      maneuver: step?.maneuver ?? NavigationManeuver.straight,
      distanceToManeuverM: progress.distanceToStepM,
      roadName: step?.roadName ?? '',
      eta: DateTime.now().add(Duration(seconds: seconds.round())),
      remainingDistanceKm: remaining / 1000,
      routeValid: true,
      source: NavigationSource.osm,
      updatedAt: DateTime.now(),
    );
  }

  void _trackOffRoute(RouteProgress progress, LatLng here) {
    if (_busy || _rerouting) return;
    final now = DateTime.now();
    if (progress.offRouteM <= _offRouteLimitM) {
      _offRouteSince = null;
      return;
    }
    _offRouteSince ??= now;
    if (now.difference(_offRouteSince!) < _offRouteHold) return;
    final last = _lastReroute;
    if (last != null && now.difference(last) < _rerouteCooldown) return;
    _lastReroute = now;
    _offRouteSince = null;
    _rerouting = true;
    unawaited(_reroute(here));
  }

  Future<void> _reroute(LatLng from) async {
    final plan = _plan;
    final destination = _destination;
    if (plan == null || destination == null) {
      _rerouting = false;
      return;
    }
    setState(() => _status = 'Off route - rerouting');
    try {
      final next = await _osm.route(
        from: from,
        to: destination.point,
        destinationName: destination.name,
      );
      if (!mounted) return;
      setState(() {
        _plan = next;
        _arrived = false;
        _status = null;
      });
      _publish(next.progressAt(from));
    } on OsmException catch (error) {
      if (!mounted) return;
      setState(() => _status = error.message);
    } catch (_) {
      if (!mounted) return;
      setState(() => _status = 'Reroute failed');
    } finally {
      _rerouting = false;
    }
  }

  Future<void> _openSearch() async {
    final query = await showDialog<String>(
      context: context,
      builder: (context) => const _SearchDialog(),
    );
    if (query == null || query.trim().isEmpty || !mounted) return;
    setState(() {
      _busy = true;
      _status = 'Searching';
      _error = null;
    });
    try {
      final fix = _gps.position;
      final places = await _osm.search(
        query.trim(),
        near: fix == null ? null : LatLng(fix.latitude, fix.longitude),
      );
      if (!mounted) return;
      if (places.isEmpty) {
        setState(() {
          _status = 'No places found';
          _busy = false;
        });
        return;
      }
      final place = await showDialog<PlaceResult>(
        context: context,
        builder: (context) => _PlaceDialog(places: places),
      );
      setState(() => _busy = false);
      if (place == null || !mounted) {
        setState(() => _status = null);
        return;
      }
      await _routeTo(place);
    } on OsmException catch (error) {
      if (!mounted) return;
      setState(() {
        _status = error.message;
        _busy = false;
      });
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _status = 'Place search failed';
        _busy = false;
      });
    }
  }

  Future<void> _routeTo(PlaceResult place) async {
    final fix = _gps.position;
    setState(() {
      _destination = place;
      _error = null;
    });
    if (fix == null) {
      _pendingDestination = place.point;
      setState(() => _status = 'Waiting for a GPS fix');
      return;
    }
    setState(() {
      _busy = true;
      _status = 'Calculating route';
    });
    try {
      final plan = await _osm.route(
        from: LatLng(fix.latitude, fix.longitude),
        to: place.point,
        destinationName: place.name,
      );
      if (!mounted) return;
      setState(() {
        _plan = plan;
        _arrived = false;
        _busy = false;
        _status = null;
        _follow = true;
      });
      _fitRoute(plan.points);
      _publish(plan.progressAt(LatLng(fix.latitude, fix.longitude)));
    } on OsmException catch (error) {
      if (!mounted) return;
      setState(() {
        _status = error.message;
        _busy = false;
      });
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _status = 'Route failed';
        _busy = false;
      });
    }
  }

  void _fitRoute(List<LatLng> points) {
    if (points.length < 2) return;
    _map.fitCamera(
      CameraFit.bounds(
        bounds: LatLngBounds.fromPoints(points),
        padding: const EdgeInsets.fromLTRB(36, 120, 36, 220),
      ),
    );
  }

  void _stop() {
    final source = _navigation.state.source;
    setState(() {
      _plan = null;
      _destination = null;
      _pendingDestination = null;
      _arrived = false;
      _status = null;
    });
    if (source != NavigationSource.googleCompanion) {
      _navigation.clearGuidance();
    }
    _recenter();
  }

  void _recenter() {
    final fix = _gps.position;
    if (fix == null) return;
    setState(() => _follow = true);
    _map.move(LatLng(fix.latitude, fix.longitude), 16);
  }

  @override
  Widget build(BuildContext context) {
    final fix = _gps.position;
    final center = fix == null
        ? const LatLng(20, 0)
        : LatLng(fix.latitude, fix.longitude);
    return Stack(
      children: [
        Positioned.fill(
          child: FlutterMap(
            mapController: _map,
            options: MapOptions(
              initialCenter: center,
              initialZoom: fix == null ? 2.4 : 16,
              minZoom: 2,
              maxZoom: 19,
              interactionOptions: const InteractionOptions(
                flags: InteractiveFlag.all & ~InteractiveFlag.rotate,
              ),
              onPositionChanged: (camera, hasGesture) {
                if (hasGesture && _follow) {
                  setState(() => _follow = false);
                }
              },
            ),
            children: [
              _tiles(),
              if (_gps.trail.length > 1)
                PolylineLayer(
                  polylines: [
                    Polyline(
                      points: _gps.trail
                          .map((point) => LatLng(point.latitude, point.longitude))
                          .toList(growable: false),
                      strokeWidth: 4,
                      color: MotoColors.amber.withValues(alpha: 0.7),
                    ),
                  ],
                ),
              if (_plan != null)
                PolylineLayer(
                  polylines: [
                    Polyline(
                      points: _plan!.points,
                      strokeWidth: 7,
                      color: MotoColors.cyan,
                      borderColor: const Color(0xCC00222C),
                      borderStrokeWidth: 2,
                    ),
                  ],
                ),
              if (_destination != null)
                MarkerLayer(
                  markers: [
                    Marker(
                      point: _destination!.point,
                      width: 34,
                      height: 34,
                      child: const Icon(
                        Icons.place,
                        color: MotoColors.red,
                        size: 32,
                      ),
                    ),
                  ],
                ),
              if (fix != null)
                MarkerLayer(
                  markers: [
                    Marker(
                      point: LatLng(fix.latitude, fix.longitude),
                      width: 44,
                      height: 44,
                      child: _PositionDot(heading: fix.heading),
                    ),
                  ],
                ),
            ],
          ),
        ),
        Positioned(
          top: 92,
          right: 12,
          child: IgnorePointer(
            child: Text(
              '© OpenStreetMap',
              style: TextStyle(
                fontSize: 9,
                color: MotoColors.text.withValues(alpha: 0.55),
                shadows: const [
                  Shadow(color: Colors.black, blurRadius: 3),
                ],
              ),
            ),
          ),
        ),
        SafeArea(
          child: Column(
            children: [
              _TopBar(
                busy: _busy,
                status: _status,
                error: _error,
                destination: _destination?.name,
                onSearch: _openSearch,
              ),
              if (_android && !_maps.enabled)
                _NoticeBanner(
                  text: 'Allow notification access for Google Maps guidance',
                  onTap: _maps.openSettings,
                ),
              const Spacer(),
              AnimatedBuilder(
                animation: _navigation,
                builder: (context, _) => _GuidancePanel(
                  state: _navigation.state,
                  gps: _gps,
                  companionStale: _maps.stale,
                  hasPlan: _plan != null,
                  guiding: _plan != null || _navigation.state.routeValid,
                  arrived: _arrived,
                  nightMode: _night,
                  onStop: _stop,
                  onRecenter: _recenter,
                  onNight: () => setState(() => _night = !_night),
                ),
              ),
              const SizedBox(height: 8),
            ],
          ),
        ),
      ],
    );
  }

  Widget _tiles() {
    const template =
        'https://tile.openstreetmap.org/{z}/{x}/{y}.png';
    if (!_night) {
      return TileLayer(
        urlTemplate: template,
        userAgentPackageName: 'com.minimotonav.moto_companion',
        maxZoom: 19,
      );
    }
    return ColorFiltered(
      colorFilter: const ColorFilter.matrix(<double>[
        -1, 0, 0, 0, 255,
        0, -1, 0, 0, 255,
        0, 0, -1, 0, 255,
        0, 0, 0, 1, 0,
      ]),
      child: TileLayer(
        urlTemplate: template,
        userAgentPackageName: 'com.minimotonav.moto_companion',
        maxZoom: 19,
      ),
    );
  }
}

class _PositionDot extends StatelessWidget {
  const _PositionDot({required this.heading});

  final double heading;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Container(
        decoration: const BoxDecoration(
          color: Color(0x334DD0E1),
          shape: BoxShape.circle,
        ),
        child: Transform.rotate(
          angle: heading * math.pi / 180,
          child: const Icon(
            Icons.navigation,
            color: MotoColors.cyan,
            size: 24,
          ),
        ),
      ),
    );
  }
}

class _TopBar extends StatelessWidget {
  const _TopBar({
    required this.busy,
    required this.status,
    required this.error,
    required this.destination,
    required this.onSearch,
  });

  final bool busy;
  final String? status;
  final String? error;
  final String? destination;
  final VoidCallback onSearch;

  @override
  Widget build(BuildContext context) {
    final gps = GpsService.instance;
    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 8, 12, 0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Material(
            color: const Color(0xE60C1218),
            borderRadius: BorderRadius.circular(14),
            child: InkWell(
              borderRadius: BorderRadius.circular(14),
              onTap: busy ? null : onSearch,
              child: Padding(
                padding: const EdgeInsets.symmetric(
                  horizontal: 14,
                  vertical: 13,
                ),
                child: Row(
                  children: [
                    Icon(
                      busy ? Icons.hourglass_top : Icons.search,
                      size: 19,
                      color: MotoColors.cyan,
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: Text(
                        destination ?? 'Search destination',
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: const TextStyle(
                          fontSize: 13.5,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ),
                    if (status != null)
                      Flexible(
                        child: Text(
                          status!,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          textAlign: TextAlign.right,
                          style: const TextStyle(
                            fontSize: 10,
                            color: MotoColors.amber,
                          ),
                        ),
                      ),
                  ],
                ),
              ),
            ),
          ),
          if (error != null) ...[
            const SizedBox(height: 6),
            Text(
              error!,
              style: const TextStyle(fontSize: 10, color: MotoColors.red),
            ),
          ],
          const SizedBox(height: 6),
          Row(
            children: [
              Text(
                gps.status.toUpperCase(),
                style: TextStyle(
                  fontSize: 9,
                  letterSpacing: 1.4,
                  color: gps.hasFix ? MotoColors.cyan : MotoColors.faint,
                ),
              ),
              const Spacer(),
              if (gps.position != null)
                Text(
                  '${gps.position!.latitude.toStringAsFixed(5)}, '
                  '${gps.position!.longitude.toStringAsFixed(5)}',
                  style: const TextStyle(
                    fontSize: 9,
                    color: MotoColors.muted,
                    fontFamily: 'monospace',
                  ),
                ),
            ],
          ),
        ],
      ),
    );
  }
}

class _NoticeBanner extends StatelessWidget {
  const _NoticeBanner({required this.text, required this.onTap});

  final String text;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 6, 12, 0),
      child: Material(
        color: const Color(0xF20C1218),
        borderRadius: BorderRadius.circular(12),
        child: InkWell(
          borderRadius: BorderRadius.circular(12),
          onTap: onTap,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 9),
            child: Row(
              children: [
                const Icon(
                  Icons.notifications_active_outlined,
                  size: 16,
                  color: MotoColors.amber,
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    text,
                    style: const TextStyle(
                      fontSize: 10.5,
                      color: MotoColors.muted,
                    ),
                  ),
                ),
                const Icon(
                  Icons.chevron_right,
                  size: 16,
                  color: MotoColors.faint,
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _GuidancePanel extends StatelessWidget {
  const _GuidancePanel({
    required this.state,
    required this.gps,
    required this.companionStale,
    required this.hasPlan,
    required this.guiding,
    required this.arrived,
    required this.nightMode,
    required this.onStop,
    required this.onRecenter,
    required this.onNight,
  });

  final NavigationState state;
  final GpsService gps;
  final bool companionStale;
  final bool hasPlan;
  final bool guiding;
  final bool arrived;
  final bool nightMode;
  final VoidCallback onStop;
  final VoidCallback onRecenter;
  final VoidCallback onNight;

  @override
  Widget build(BuildContext context) {
    final source = state.source;
    final google = source == NavigationSource.googleCompanion;
    final stale = google && companionStale;
    final age = state.updatedAt == null
        ? null
        : DateTime.now().difference(state.updatedAt!).inSeconds;
    final title = switch (source) {
      NavigationSource.osm =>
        arrived ? 'Arrived' : (state.roadName.isEmpty ? 'Following route' : state.roadName),
      NavigationSource.googleCompanion =>
        state.roadName.isEmpty ? 'Google Maps guidance' : state.roadName,
      NavigationSource.none => 'Ready to navigate',
    };
    final subtitle = switch (source) {
      NavigationSource.osm => arrived
          ? 'Destination reached'
          : '${_meters(state.distanceToManeuverM)} to next maneuver',
      NavigationSource.googleCompanion => stale
          ? 'Google Maps guidance is stale'
          : '${_meters(state.distanceToManeuverM)} to next maneuver',
      NavigationSource.none => 'Search a destination to start',
    };
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 12),
      child: Container(
        padding: const EdgeInsets.fromLTRB(14, 12, 14, 10),
        decoration: BoxDecoration(
          color: const Color(0xF20C1218),
          borderRadius: BorderRadius.circular(16),
          border: Border.all(
            color: stale ? MotoColors.amber : MotoColors.stroke,
          ),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(
                  _maneuverIcon(state.maneuver),
                  color: stale ? MotoColors.amber : MotoColors.cyan,
                  size: 26,
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        title,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: const TextStyle(
                          fontSize: 14,
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                      const SizedBox(height: 2),
                      Text(
                        subtitle,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: TextStyle(
                          fontSize: 10.5,
                          color: stale ? MotoColors.amber : MotoColors.muted,
                        ),
                      ),
                    ],
                  ),
                ),
                if (source != NavigationSource.none)
                  Text(
                    _sourceLabel(source, age, stale),
                    textAlign: TextAlign.right,
                    style: const TextStyle(
                      fontSize: 8,
                      letterSpacing: 1,
                      color: MotoColors.faint,
                    ),
                  ),
                if (state.remainingDistanceKm > 0 || hasPlan)
                  Padding(
                    padding: const EdgeInsets.only(left: 10),
                    child: Text(
                      state.remainingDistanceKm >= 1
                          ? '${state.remainingDistanceKm.toStringAsFixed(1)} km'
                          : _meters(state.remainingDistanceKm * 1000),
                      style: const TextStyle(
                        fontSize: 15,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 10),
            Row(
              children: [
                _Readout(
                  label: 'SPEED',
                  value: gps.speedKph.toStringAsFixed(0),
                  unit: 'km/h',
                ),
                _Readout(
                  label: 'ETA',
                  value: _clock(state.eta),
                  unit: '',
                ),
                _Readout(
                  label: 'ACCURACY',
                  value: (gps.position?.accuracy ?? 0).toStringAsFixed(1),
                  unit: 'm',
                ),
                _Readout(
                  label: 'TRIP',
                  value: gps.tripKm.toStringAsFixed(2),
                  unit: 'km',
                ),
                IconButton(
                  tooltip: nightMode ? 'Day map' : 'Night map',
                  onPressed: onNight,
                  icon: Icon(
                    nightMode ? Icons.light_mode : Icons.dark_mode,
                    size: 18,
                  ),
                ),
                IconButton(
                  tooltip: 'Recenter',
                  onPressed: onRecenter,
                  icon: const Icon(Icons.my_location, size: 18),
                ),
                if (guiding && !google)
                  IconButton(
                    tooltip: 'Stop guidance',
                    onPressed: onStop,
                    icon: const Icon(Icons.close, size: 18),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  static String _sourceLabel(
    NavigationSource source,
    int? age,
    bool stale,
  ) {
    final name = source == NavigationSource.osm ? 'OSM' : 'GOOGLE';
    if (stale) return '$name\nSTALE';
    if (age == null) return name;
    return '$name\n${age}s';
  }

  static String _clock(DateTime? eta) {
    if (eta == null) return '--:--';
    return '${eta.hour.toString().padLeft(2, '0')}:'
        '${eta.minute.toString().padLeft(2, '0')}';
  }

  static String _meters(num? meters) {
    if (meters == null) return '--';
    if (meters >= 1000) return '${(meters / 1000).toStringAsFixed(1)} km';
    return '${meters.round()} m';
  }

  static IconData _maneuverIcon(NavigationManeuver maneuver) {
    return switch (maneuver) {
      NavigationManeuver.none => Icons.navigation,
      NavigationManeuver.straight => Icons.straight,
      NavigationManeuver.slightLeft => Icons.turn_slight_left,
      NavigationManeuver.left => Icons.turn_left,
      NavigationManeuver.sharpLeft => Icons.turn_sharp_left,
      NavigationManeuver.slightRight => Icons.turn_slight_right,
      NavigationManeuver.right => Icons.turn_right,
      NavigationManeuver.sharpRight => Icons.turn_sharp_right,
      NavigationManeuver.uTurn => Icons.u_turn_left,
      NavigationManeuver.roundabout => Icons.roundabout_right,
      NavigationManeuver.arrive => Icons.place,
    };
  }
}

class _Readout extends StatelessWidget {
  const _Readout({required this.label, required this.value, required this.unit});

  final String label;
  final String value;
  final String unit;

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            label,
            style: const TextStyle(
              fontSize: 8,
              letterSpacing: 1.1,
              color: MotoColors.faint,
            ),
          ),
          const SizedBox(height: 2),
          Row(
            crossAxisAlignment: CrossAxisAlignment.baseline,
            textBaseline: TextBaseline.alphabetic,
            children: [
              Flexible(
                child: Text(
                  value,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(
                    fontSize: 15,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ),
              if (unit.isNotEmpty) ...[
                const SizedBox(width: 3),
                Text(
                  unit,
                  style: const TextStyle(fontSize: 8, color: MotoColors.faint),
                ),
              ],
            ],
          ),
        ],
      ),
    );
  }
}

class _SearchDialog extends StatefulWidget {
  const _SearchDialog();

  @override
  State<_SearchDialog> createState() => _SearchDialogState();
}

class _SearchDialogState extends State<_SearchDialog> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Destination'),
      content: TextField(
        controller: _controller,
        autofocus: true,
        textInputAction: TextInputAction.search,
        decoration: const InputDecoration(
          hintText: 'Search an address or place',
        ),
        onSubmitted: (value) => Navigator.of(context).pop(value),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('CANCEL'),
        ),
        FilledButton(
          onPressed: () => Navigator.of(context).pop(_controller.text),
          child: const Text('SEARCH'),
        ),
      ],
    );
  }
}

class _PlaceDialog extends StatelessWidget {
  const _PlaceDialog({required this.places});

  final List<PlaceResult> places;

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Choose destination'),
      content: SizedBox(
        width: double.maxFinite,
        height: 360,
        child: ListView.separated(
          itemCount: places.length,
          separatorBuilder: (_, _) => const Divider(height: 1),
          itemBuilder: (context, index) {
            final place = places[index];
            return ListTile(
              dense: true,
              leading: const Icon(Icons.place_outlined, size: 20),
              title: Text(
                place.name,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
              subtitle: Text(
                place.address,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
              ),
              onTap: () => Navigator.of(context).pop(place),
            );
          },
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('BACK'),
        ),
      ],
    );
  }
}
