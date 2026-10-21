import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../app/theme.dart';
import '../gps_service.dart';
import '../protocol_service.dart';
import '../services/calibration_store.dart';
import '../widgets/common.dart';

class _LogSample {
  const _LogSample(this.at, this.values);

  final DateTime at;
  final Map<String, double> values;
}

class LogScreen extends StatefulWidget {
  const LogScreen({super.key});

  @override
  State<LogScreen> createState() => _LogScreenState();
}

class _LogScreenState extends State<LogScreen> {
  final List<_LogSample> _samples = [];
  Timer? _timer;
  bool _recording = false;

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  void _toggle() {
    if (_recording) {
      _timer?.cancel();
      _timer = null;
      setState(() => _recording = false);
      return;
    }
    setState(() => _recording = true);
    _timer = Timer.periodic(const Duration(milliseconds: 500), (_) {
      final values = Map<String, double>.from(ProtocolService.instance.values);
      values['GPS_Speed'] = GpsService.instance.speedKph;
      if (values.isEmpty) return;
      setState(() {
        _samples.add(_LogSample(DateTime.now(), values));
        if (_samples.length > 2000) _samples.removeAt(0);
      });
    });
  }

  Future<void> _export() async {
    if (_samples.isEmpty) return;
    final payload = {
      'schema': 'minimotonav.log.v1',
      'exportedAt': DateTime.now().toIso8601String(),
      'samples': [
        for (final sample in _samples)
          {'t': sample.at.toIso8601String(), 'values': sample.values},
      ],
    };
    await Clipboard.setData(ClipboardData(text: jsonEncode(payload)));
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('${_samples.length} samples copied as JSON')),
    );
  }

  @override
  Widget build(BuildContext context) {
    final protocol = ProtocolService.instance;
    final rpm = _samples.map((sample) => sample.values['RPM'] ?? 0).toList();
    return SafeArea(
      bottom: false,
      child: Column(
        children: [
          PageHeader(
            title: 'Log View',
            subtitle: 'Live recorder · JSON export',
            trailing: StatusPill(
              label: _recording ? 'Recording' : 'Idle',
              color: _recording ? MotoColors.red : MotoColors.faint,
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: Panel(
              child: Column(
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: FilledButton.icon(
                          onPressed: _toggle,
                          icon: Icon(
                            _recording ? Icons.stop : Icons.fiber_manual_record,
                            size: 16,
                          ),
                          label: Text(_recording ? 'STOP' : 'RECORD'),
                          style: FilledButton.styleFrom(
                            backgroundColor: _recording
                                ? MotoColors.red
                                : MotoColors.cyan,
                          ),
                        ),
                      ),
                      const SizedBox(width: 8),
                      OutlinedButton(
                        onPressed: _samples.isEmpty ? null : _export,
                        child: const Text('EXPORT'),
                      ),
                      const SizedBox(width: 8),
                      OutlinedButton(
                        onPressed: _samples.isEmpty
                            ? null
                            : () => setState(_samples.clear),
                        child: const Text('CLEAR'),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  Row(
                    children: [
                      _stat('SAMPLES', '${_samples.length}'),
                      _stat('RATE', '2 Hz'),
                      _stat(
                        'SOURCE',
                        protocol.connected ? 'ECU + GPS' : 'GPS / preview',
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 10),
          Expanded(
            child: _samples.isEmpty
                ? EmptyState(
                    icon: Icons.multiline_chart,
                    title: protocol.connected
                        ? 'Ready to record'
                        : 'No link, no samples',
                    body: protocol.connected
                        ? 'Samples capture every subscribed channel at 2 Hz.'
                        : 'Connect the ECU or enable Preview in Settings. Recorder only stores real channel maps.',
                  )
                : ListView(
                    padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
                    children: [
                      Panel(
                        padding: const EdgeInsets.fromLTRB(10, 12, 10, 8),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Padding(
                              padding: const EdgeInsets.symmetric(
                                horizontal: 4,
                              ),
                              child: Text(
                                'RPM TRACE',
                                style: labelStyle(size: 9),
                              ),
                            ),
                            const SizedBox(height: 8),
                            SizedBox(
                              height: 90,
                              child: CustomPaint(
                                painter: _TracePainter(rpm),
                                child: const SizedBox.expand(),
                              ),
                            ),
                          ],
                        ),
                      ),
                      const SizedBox(height: 10),
                      for (final sample in _samples.reversed.take(24))
                        Padding(
                          padding: const EdgeInsets.only(bottom: 6),
                          child: Panel(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 9,
                            ),
                            child: Row(
                              children: [
                                Text(
                                  '${sample.at.hour.toString().padLeft(2, "0")}:${sample.at.minute.toString().padLeft(2, "0")}:${sample.at.second.toString().padLeft(2, "0")}',
                                  style: monoStyle(
                                    size: 10,
                                    color: MotoColors.cyan,
                                  ),
                                ),
                                const SizedBox(width: 10),
                                Expanded(
                                  child: Text(
                                    sample.values.entries
                                        .map(
                                          (e) =>
                                              '${e.key}=${e.value.toStringAsFixed(1)}',
                                        )
                                        .join('  '),
                                    maxLines: 1,
                                    overflow: TextOverflow.ellipsis,
                                    style: monoStyle(
                                      size: 10,
                                      color: MotoColors.muted,
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                    ],
                  ),
          ),
        ],
      ),
    );
  }

  Widget _stat(String label, String value) {
    return Expanded(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label, style: labelStyle(size: 8)),
          const SizedBox(height: 2),
          Text(value, style: monoStyle(size: 13)),
        ],
      ),
    );
  }
}

class _TracePainter extends CustomPainter {
  _TracePainter(this.values);

  final List<double> values;

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = MotoColors.cyan
      ..strokeWidth = 1.6
      ..style = PaintingStyle.stroke;
    final path = Path();
    final max = 16000.0;
    for (var i = 0; i < values.length; i++) {
      final x = values.length == 1 ? 0.0 : size.width * i / (values.length - 1);
      final y = size.height - (values[i].clamp(0, max) / max) * size.height;
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    canvas.drawPath(path, paint);
  }

  @override
  bool shouldRepaint(_TracePainter old) => old.values.length != values.length;
}

class _TestRun {
  _TestRun(this.at, this.time60, this.time100);

  final DateTime at;
  final double? time60;
  final double? time100;
}

class PowerTestScreen extends StatefulWidget {
  const PowerTestScreen({super.key});

  @override
  State<PowerTestScreen> createState() => _PowerTestScreenState();
}

class _PowerTestScreenState extends State<PowerTestScreen> {
  final List<_TestRun> _runs = [];
  final _gps = GpsService.instance;
  Timer? _ticker;
  DateTime? _started;
  double? _time60;
  double? _time100;
  bool _armed = false;
  bool _running = false;

  @override
  void initState() {
    super.initState();
    _gps.addListener(_onGps);
  }

  @override
  void dispose() {
    _gps.removeListener(_onGps);
    _ticker?.cancel();
    super.dispose();
  }

  void _onGps() {
    if (!_armed || !_gps.hasFix) return;
    final speed = _gps.speedKph;
    if (!_running && speed > 2) {
      setState(() {
        _running = true;
        _started = DateTime.now();
      });
      _ticker?.cancel();
      _ticker = Timer.periodic(const Duration(milliseconds: 100), (_) {
        if (mounted) setState(() {});
      });
      return;
    }
    if (!_running || _started == null) return;
    final elapsed = DateTime.now().difference(_started!).inMilliseconds / 1000;
    if (_time60 == null && speed >= 60) {
      setState(() => _time60 = elapsed);
    }
    if (_time100 == null && speed >= 100) {
      setState(() {
        _time100 = elapsed;
        _finish();
      });
    }
  }

  void _arm() {
    _ticker?.cancel();
    setState(() {
      _armed = !_armed;
      _running = false;
      _started = null;
      _time60 = null;
      _time100 = null;
    });
  }

  void _finish() {
    if (_started != null) {
      _runs.insert(0, _TestRun(DateTime.now(), _time60, _time100));
    }
    _armed = false;
    _running = false;
    _ticker?.cancel();
    _ticker = null;
  }

  String get _elapsed {
    if (_started == null) return '0.00';
    return (DateTime.now().difference(_started!).inMilliseconds / 1000)
        .toStringAsFixed(2);
  }

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      bottom: false,
      child: AnimatedBuilder(
        animation: _gps,
        builder: (context, _) {
          final speed = _gps.hasFix ? _gps.speedKph : 0.0;
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 28),
            children: [
              const PageHeader(
                title: 'Power Test',
                subtitle: 'GPS acceleration timing · no dyno claim',
              ),
              const InfoBanner(
                text: 'Times are derived from device GPS, not ECU speed or a dyno. Accuracy depends on the fix; treat results as relative.',
                color: MotoColors.amber,
              ),
              const SizedBox(height: 12),
              Panel(
                padding: const EdgeInsets.all(18),
                child: Column(
                  children: [
                    Text(
                      _running
                          ? 'RUNNING'
                          : (_armed ? 'ARMED — LAUNCH WHEN READY' : 'STANDBY'),
                      style: labelStyle(),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      _elapsed,
                      style: monoStyle(
                        size: 58,
                        weight: FontWeight.w700,
                        color: MotoColors.cyan,
                      ),
                    ),
                    Text(
                      'SECONDS',
                      style: labelStyle(size: 9, color: MotoColors.faint),
                    ),
                    const SizedBox(height: 16),
                    Row(
                      children: [
                        _split('0–60', _time60),
                        _split('0–100', _time100),
                        _split('GPS', _gps.hasFix ? speed : null),
                      ],
                    ),
                    const SizedBox(height: 14),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton(
                        onPressed: _gps.hasFix
                            ? (_running ? _finish : _arm)
                            : null,
                        style: FilledButton.styleFrom(
                          backgroundColor: _armed
                              ? MotoColors.amber
                              : MotoColors.cyan,
                        ),
                        child: Text(
                          _running
                              ? 'FINISH RUN'
                              : (_armed ? 'DISARM' : 'ARM RUN'),
                        ),
                      ),
                    ),
                    if (!_gps.hasFix) ...[
                      const SizedBox(height: 8),
                      Text(
                        'Waiting for GPS fix (${_gps.status})',
                        style: const TextStyle(
                          fontSize: 11,
                          color: MotoColors.faint,
                        ),
                      ),
                    ],
                  ],
                ),
              ),
              SectionTitle(
                'Runs',
                trailing: Text(
                  '${_runs.length} recorded',
                  style: labelStyle(size: 9),
                ),
              ),
              if (_runs.isEmpty)
                const EmptyState(
                  icon: Icons.timer_outlined,
                  title: 'No runs yet',
                  body: 'Arm the timer, then accelerate through 100 km/h.',
                ),
              ..._runs
                  .take(8)
                  .map(
                    (run) => Padding(
                      padding: const EdgeInsets.only(bottom: 6),
                      child: Panel(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 12,
                          vertical: 10,
                        ),
                        child: Row(
                          children: [
                            Text(
                              '${run.at.hour.toString().padLeft(2, "0")}:${run.at.minute.toString().padLeft(2, "0")}',
                              style: monoStyle(
                                size: 10,
                                color: MotoColors.muted,
                              ),
                            ),
                            const Spacer(),
                            Text(
                              '60: ${run.time60?.toStringAsFixed(2) ?? "--"}s',
                              style: monoStyle(size: 11),
                            ),
                            const SizedBox(width: 12),
                            Text(
                              '100: ${run.time100?.toStringAsFixed(2) ?? "--"}s',
                              style: monoStyle(
                                size: 11,
                                color: MotoColors.cyan,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
            ],
          );
        },
      ),
    );
  }

  Widget _split(String label, double? value) {
    return Expanded(
      child: Column(
        children: [
          Text(label, style: labelStyle(size: 9)),
          const SizedBox(height: 3),
          Text(
            value == null ? '--' : value.toStringAsFixed(value > 100 ? 0 : 2),
            style: monoStyle(size: 20),
          ),
        ],
      ),
    );
  }
}

class MapStoreScreen extends StatefulWidget {
  const MapStoreScreen({super.key});

  @override
  State<MapStoreScreen> createState() => _MapStoreScreenState();
}

class _MapStoreScreenState extends State<MapStoreScreen> {
  final _store = CalibrationStore.instance;

  @override
  void initState() {
    super.initState();
    _store.ensureLoaded();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _store,
      builder: (context, _) {
        return SafeArea(
          bottom: false,
          child: Column(
            children: [
              PageHeader(
                title: 'Map Store',
                subtitle: 'On-device calibration snapshots',
                trailing: FilledButton.icon(
                  onPressed: _store.catalog == null
                      ? null
                      : () => _save(context),
                  icon: const Icon(Icons.add, size: 16),
                  label: const Text('SAVE'),
                  style: FilledButton.styleFrom(minimumSize: const Size(0, 40)),
                ),
              ),
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16),
                child: const InfoBanner(
                  text: 'Maps are local JSON snapshots of preview values. They are not ECU binaries and cannot be flashed.',
                  color: MotoColors.cyan,
                ),
              ),
              const SizedBox(height: 10),
              Expanded(
                child: _store.maps.isEmpty
                    ? const EmptyState(
                        icon: Icons.folder_open,
                        title: 'No saved maps',
                        body: 'Save the current calibration edits as a named snapshot.',
                      )
                    : ListView.separated(
                        padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
                        itemCount: _store.maps.length,
                        separatorBuilder: (_, _) => const SizedBox(height: 8),
                        itemBuilder: (context, index) {
                          final slot = _store.maps[index];
                          return Panel(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Row(
                                  children: [
                                    const Icon(
                                      Icons.map_outlined,
                                      size: 18,
                                      color: MotoColors.cyan,
                                    ),
                                    const SizedBox(width: 8),
                                    Expanded(
                                      child: Text(
                                        slot.name,
                                        style: const TextStyle(
                                          fontSize: 14,
                                          fontWeight: FontWeight.w600,
                                        ),
                                      ),
                                    ),
                                    CapabilityBadge(
                                      '${slot.changed} tabs',
                                      color: MotoColors.faint,
                                    ),
                                  ],
                                ),
                                const SizedBox(height: 4),
                                Text(
                                  '${slot.savedAt.toString().substring(0, 16)} · local snapshot',
                                  style: const TextStyle(
                                    fontSize: 10.5,
                                    color: MotoColors.muted,
                                  ),
                                ),
                                const SizedBox(height: 10),
                                Row(
                                  children: [
                                    OutlinedButton(
                                      onPressed: () async {
                                        await _store.applyMap(slot);
                                        if (!context.mounted) return;
                                        ScaffoldMessenger.of(context)
                                            .showSnackBar(
                                              SnackBar(
                                                content: Text(
                                                  'Loaded ${slot.name}',
                                                ),
                                              ),
                                            );
                                      },
                                      child: const Text('LOAD'),
                                    ),
                                    const SizedBox(width: 8),
                                    TextButton(
                                      onPressed: () => _store.deleteMap(slot),
                                      child: const Text(
                                        'DELETE',
                                        style: TextStyle(color: MotoColors.red),
                                      ),
                                    ),
                                  ],
                                ),
                              ],
                            ),
                          );
                        },
                      ),
              ),
            ],
          ),
        );
      },
    );
  }

  Future<void> _save(BuildContext context) async {
    final controller = TextEditingController(
      text: 'Map ${_store.maps.length + 1}',
    );
    final name = await showDialog<String>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: MotoColors.panel,
        title: const Text('Save calibration snapshot'),
        content: TextField(
          controller: controller,
          autofocus: true,
          decoration: const InputDecoration(labelText: 'Map name'),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('CANCEL'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, controller.text),
            child: const Text('SAVE'),
          ),
        ],
      ),
    );
    controller.dispose();
    if (name == null) return;
    await _store.saveMap(name);
  }
}
