import 'package:flutter/material.dart';

import '../app/theme.dart';
import '../gps_service.dart';
import '../protocol_service.dart';
import '../services/app_settings.dart';
import '../telemetry/telemetry_service.dart';
import '../widgets/arc_gauge.dart';
import '../widgets/common.dart';

enum MonitorStyle { simple, race, cube }

extension MonitorStyleInfo on MonitorStyle {
  String get title => switch (this) {
    MonitorStyle.simple => 'Simple Monitor',
    MonitorStyle.race => 'Race Monitor',
    MonitorStyle.cube => 'Cube Monitor',
  };
  String get tag => switch (this) {
    MonitorStyle.simple => 'LITE',
    MonitorStyle.race => 'PRO',
    MonitorStyle.cube => 'QUICK',
  };
  IconData get icon => switch (this) {
    MonitorStyle.simple => Icons.speed,
    MonitorStyle.race => Icons.sports_motorsports,
    MonitorStyle.cube => Icons.grid_view_rounded,
  };
}

class MonitorScreen extends StatelessWidget {
  const MonitorScreen({super.key, required this.style});

  final MonitorStyle style;

  @override
  Widget build(BuildContext context) {
    final settings = AppSettings.instance;
    final protocol = ProtocolService.instance;
    final gps = GpsService.instance;
    final telemetry = TelemetryService.instance;

    return AnimatedBuilder(
      animation: Listenable.merge([protocol, gps, settings, telemetry]),
      builder: (context, _) {
        final demo = settings.preview;
        final values = telemetry.values;
        final kph = gps.hasFix ? gps.speedKph : 0.0;
        final hasTelemetry = telemetry.hasData;

        return SafeArea(
          bottom: false,
          child: Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 980),
              child: ListView(
                padding: const EdgeInsets.fromLTRB(14, 10, 14, 24),
                children: [
                  _LinkStrip(
                    demo: demo,
                    live: protocol.connected,
                    hasTelemetry: hasTelemetry,
                    count: telemetry.telemetryCount,
                    gpsStatus: gps.status,
                    onPreview: () => settings.setPreview(!demo),
                  ),
                  const SizedBox(height: 12),
                  if (telemetry.stale) ...[
                    const InfoBanner(
                      text: 'ECU telemetry is stale. Last values are shown and are not live.',
                      icon: Icons.schedule,
                      color: MotoColors.amber,
                    ),
                    const SizedBox(height: 12),
                  ],
                  if (demo) ...[
                    const InfoBanner(
                      text: 'PREVIEW data is synthetic and exists for layout review. It is not an ECU reading.',
                      icon: Icons.science_outlined,
                      color: MotoColors.amber,
                    ),
                    const SizedBox(height: 12),
                  ],
                  switch (style) {
                    MonitorStyle.simple => _SimpleLayout(
                      values: values,
                      kph: kph,
                      hasData: hasTelemetry,
                    ),
                    MonitorStyle.race => _RaceLayout(
                      values: values,
                      kph: kph,
                      hasData: hasTelemetry,
                    ),
                    MonitorStyle.cube => _CubeLayout(
                      values: values,
                      kph: kph,
                      hasData: hasTelemetry,
                    ),
                  },
                  const SizedBox(height: 14),
                  _Footer(demo: demo, values: values, gps: gps),
                ],
              ),
            ),
          ),
        );
      },
    );
  }
}

class _LinkStrip extends StatelessWidget {
  const _LinkStrip({
    required this.demo,
    required this.live,
    required this.hasTelemetry,
    required this.count,
    required this.gpsStatus,
    required this.onPreview,
  });

  final bool demo;
  final bool live;
  final bool hasTelemetry;
  final int count;
  final String gpsStatus;
  final VoidCallback onPreview;

  @override
  Widget build(BuildContext context) {
    final linkColor = demo
        ? MotoColors.amber
        : (live ? MotoColors.lime : MotoColors.faint);
    return Row(
      children: [
        StatusPill(
          label: demo ? 'Preview' : (live ? 'CAN link' : 'Offline'),
          color: linkColor,
          icon: demo ? Icons.science_outlined : Icons.link,
        ),
        const SizedBox(width: 8),
        StatusPill(
          label: gpsStatus,
          color: gpsStatus == '3d fix' ? MotoColors.cyan : MotoColors.faint,
          icon: Icons.gps_fixed,
        ),
        const Spacer(),
        if (!demo && live)
          Text(
            '${count.toString().padLeft(5, '0')} PDO',
            style: monoStyle(size: 10, color: MotoColors.faint),
          ),
        const SizedBox(width: 6),
        IconButton(
          tooltip: demo ? 'Stop preview' : 'Preview layout',
          onPressed: onPreview,
          icon: Icon(
            demo ? Icons.stop_circle_outlined : Icons.play_circle_outline,
            size: 22,
          ),
          color: demo ? MotoColors.amber : MotoColors.muted,
          visualDensity: VisualDensity.compact,
        ),
      ],
    );
  }
}

class _SimpleLayout extends StatelessWidget {
  const _SimpleLayout({
    required this.values,
    required this.kph,
    required this.hasData,
  });

  final Map<String, double> values;
  final double kph;
  final bool hasData;

  @override
  Widget build(BuildContext context) {
    final settings = AppSettings.instance;
    final rpm = values['RPM'] ?? 0;
    final speed = settings.speed(kph);
    return Column(
      children: [
        Panel(
          padding: const EdgeInsets.fromLTRB(18, 18, 18, 12),
          child: Column(
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('ENGINE SPEED', style: labelStyle()),
                        const SizedBox(height: 6),
                        Text(
                          hasData ? rpm.toStringAsFixed(0) : '--',
                          style: monoStyle(
                            size: 46,
                            weight: FontWeight.w700,
                            color: MotoColors.cyan,
                            letterSpacing: -1,
                          ),
                        ),
                        Text(
                          'RPM',
                          style: labelStyle(size: 10, color: MotoColors.faint),
                        ),
                      ],
                    ),
                  ),
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.end,
                    children: [
                      Text('GPS SPEED', style: labelStyle()),
                      const SizedBox(height: 6),
                      Text(
                        speed.toStringAsFixed(0),
                        style: monoStyle(size: 46, weight: FontWeight.w700),
                      ),
                      Text(
                        settings.speedUnit().toUpperCase(),
                        style: labelStyle(size: 10, color: MotoColors.faint),
                      ),
                    ],
                  ),
                ],
              ),
              const SizedBox(height: 16),
              _RpmBar(rpm: rpm),
              const SizedBox(height: 8),
              Row(
                children: [
                  Text('0', style: monoStyle(size: 9, color: MotoColors.faint)),
                  const Spacer(),
                  Text(
                    '16K REDLINE 13.5K',
                    style: labelStyle(size: 8, color: MotoColors.faint),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        _MetricGrid(
          tiles: [
            _Metric('TPS', values['TPS_Percent'], '%', 100),
            _Metric('COOLANT', values['T_Eng'], '°C', 130),
            _Metric('AFR TARGET', values['AFR_Target'], ':1', 18),
            _Metric('BATTERY', values['Volt_Batt'], 'V', 16),
            _Metric('IGNITION', values['SA'], '°', 60),
            _Metric('AIR TEMP', values['T_Air'], '°C', 70),
          ],
        ),
        const SizedBox(height: 12),
        _GpsPanel(gps: GpsService.instance),
      ],
    );
  }
}

class _RaceLayout extends StatelessWidget {
  const _RaceLayout({
    required this.values,
    required this.kph,
    required this.hasData,
  });

  final Map<String, double> values;
  final double kph;
  final bool hasData;

  @override
  Widget build(BuildContext context) {
    final settings = AppSettings.instance;
    final rpm = values['RPM'] ?? 0;
    final speed = settings.speed(kph);
    return Column(
      children: [
        _ShiftLights(rpm: rpm),
        const SizedBox(height: 10),
        LayoutBuilder(
          builder: (context, constraints) {
            final side = constraints.maxWidth > 560;
            final gauges = [
              _GaugeCard(
                label: 'RPM',
                value: hasData ? rpm : 0,
                display: hasData ? rpm.toStringAsFixed(0) : '--',
                max: 16000,
                redline: 13500,
                color: MotoColors.cyan,
              ),
              _GaugeCard(
                label: 'GPS SPEED',
                value: speed,
                display: speed.toStringAsFixed(0),
                max: settings.metric ? 300 : 190,
                color: MotoColors.blue,
                unit: settings.speedUnit(),
              ),
            ];
            if (!side) {
              return Column(
                children: [gauges[0], const SizedBox(height: 10), gauges[1]],
              );
            }
            return Row(
              children: [
                Expanded(child: gauges[0]),
                const SizedBox(width: 10),
                Expanded(child: gauges[1]),
              ],
            );
          },
        ),
        const SizedBox(height: 12),
        _MetricGrid(
          crossAxisCount: 3,
          tiles: [
            _Metric('TPS', values['TPS_Percent'], '%', 100),
            _Metric('AFR', values['AFR_Target'], ':1', 18),
            _Metric('SA', values['SA'], '°', 60),
            _Metric('ECT', values['T_Eng'], '°C', 130),
            _Metric('BATT', values['Volt_Batt'], 'V', 16),
            _Metric('TRIP', GpsService.instance.tripKm, 'km', 100),
          ],
        ),
      ],
    );
  }
}

class _CubeLayout extends StatelessWidget {
  const _CubeLayout({
    required this.values,
    required this.kph,
    required this.hasData,
  });

  final Map<String, double> values;
  final double kph;
  final bool hasData;

  @override
  Widget build(BuildContext context) {
    final settings = AppSettings.instance;
    final rpm = values['RPM'] ?? 0;
    final speed = settings.speed(kph);
    return Column(
      children: [
        Row(
          children: [
            Expanded(
              child: Panel(
                color: MotoColors.panelHigh,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('RPM', style: labelStyle()),
                    const SizedBox(height: 8),
                    Text(
                      hasData ? rpm.toStringAsFixed(0) : '--',
                      style: monoStyle(
                        size: 40,
                        weight: FontWeight.w700,
                        color: MotoColors.cyan,
                      ),
                    ),
                    const SizedBox(height: 8),
                    _RpmBar(rpm: rpm, height: 5),
                  ],
                ),
              ),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: Panel(
                color: MotoColors.panelHigh,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('SPEED', style: labelStyle()),
                    const SizedBox(height: 8),
                    Text(
                      speed.toStringAsFixed(0),
                      style: monoStyle(size: 40, weight: FontWeight.w700),
                    ),
                    Text(
                      settings.speedUnit().toUpperCase(),
                      style: labelStyle(size: 9, color: MotoColors.faint),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
        const SizedBox(height: 10),
        _MetricGrid(
          crossAxisCount: 2,
          tiles: [
            _Metric('TPS', values['TPS_Percent'], '%', 100),
            _Metric('COOLANT', values['T_Eng'], '°C', 130),
            _Metric('AFR TARGET', values['AFR_Target'], ':1', 18),
            _Metric('VOLTAGE', values['Volt_Batt'], 'V', 16),
            _Metric('IGNITION', values['SA'], '°', 60),
            _Metric('AIR TEMP', values['T_Air'], '°C', 70),
          ],
        ),
        const SizedBox(height: 12),
        const InfoBanner(
          text: 'Cube layout mirrors the compact QUICK monitor: six high-value channels, no charts.',
          icon: Icons.grid_view_rounded,
        ),
      ],
    );
  }
}

class _GaugeCard extends StatelessWidget {
  const _GaugeCard({
    required this.label,
    required this.value,
    required this.display,
    required this.max,
    required this.color,
    this.redline,
    this.unit = 'RPM',
  });

  final String label;
  final double value;
  final String display;
  final double max;
  final Color color;
  final double? redline;
  final String unit;

  @override
  Widget build(BuildContext context) {
    return Panel(
      child: Column(
        children: [
          Align(
            alignment: Alignment.centerLeft,
            child: Text(label, style: labelStyle()),
          ),
          const SizedBox(height: 4),
          LayoutBuilder(
            builder: (context, constraints) {
              final size = constraints.maxWidth.clamp(160.0, 300.0);
              return SizedBox(
                height: size * 0.72,
                child: Stack(
                  alignment: Alignment.center,
                  children: [
                    ArcGauge(
                      value: value,
                      max: max,
                      label: '',
                      unit: unit,
                      redline: redline,
                      size: size,
                      color: color,
                      showValue: false,
                    ),
                    Positioned(
                      bottom: 10,
                      child: Text(
                        display,
                        style: monoStyle(size: 26, weight: FontWeight.w700),
                      ),
                    ),
                  ],
                ),
              );
            },
          ),
        ],
      ),
    );
  }
}

class _RpmBar extends StatelessWidget {
  const _RpmBar({required this.rpm, this.height = 8});

  final double rpm;
  final double height;

  @override
  Widget build(BuildContext context) {
    final fraction = (rpm / 16000).clamp(0.0, 1.0);
    return LayoutBuilder(
      builder: (context, constraints) {
        const segments = 32;
        final active = (fraction * segments).ceil();
        return Row(
          children: List.generate(segments, (index) {
            final lit = index < active;
            final color = index >= 27
                ? MotoColors.red
                : index >= 22
                ? MotoColors.amber
                : MotoColors.cyan;
            return Expanded(
              child: Container(
                height: height,
                margin: EdgeInsets.only(right: index == segments - 1 ? 0 : 2),
                decoration: BoxDecoration(
                  color: lit ? color : MotoColors.stroke,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
            );
          }),
        );
      },
    );
  }
}

class _ShiftLights extends StatelessWidget {
  const _ShiftLights({required this.rpm});

  final double rpm;

  @override
  Widget build(BuildContext context) {
    final fraction = (rpm / 16000).clamp(0.0, 1.0);
    return Panel(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      child: Row(
        children: List.generate(14, (index) {
          final threshold = (index + 1) / 14;
          final lit = fraction >= threshold;
          final color = index >= 11
              ? MotoColors.red
              : index >= 8
              ? MotoColors.amber
              : MotoColors.lime;
          return Expanded(
            child: Container(
              height: 18,
              margin: const EdgeInsets.symmetric(horizontal: 2),
              decoration: BoxDecoration(
                color: lit ? color : MotoColors.stroke,
                borderRadius: BorderRadius.circular(3),
                boxShadow: lit
                    ? [
                        BoxShadow(
                          color: color.withValues(alpha: 0.5),
                          blurRadius: 8,
                        ),
                      ]
                    : null,
              ),
            ),
          );
        }),
      ),
    );
  }
}

class _Metric {
  const _Metric(this.label, this.value, this.unit, this.max);

  final String label;
  final double? value;
  final String unit;
  final double max;
}

class _MetricGrid extends StatelessWidget {
  const _MetricGrid({required this.tiles, this.crossAxisCount = 2});

  final List<_Metric> tiles;
  final int crossAxisCount;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final columns = constraints.maxWidth > 620
            ? (crossAxisCount * 2).clamp(2, 6)
            : crossAxisCount;
        const spacing = 10.0;
        final width =
            (constraints.maxWidth - spacing * (columns - 1)) / columns;
        return Wrap(
          spacing: spacing,
          runSpacing: spacing,
          children: [
            for (final tile in tiles)
              SizedBox(
                width: width,
                child: MetricTile(
                  label: tile.label,
                  value: tile.value == null ? '--' : fmt(tile.value!),
                  unit: tile.unit,
                  progress: tile.value == null
                      ? null
                      : (tile.value! / tile.max).clamp(0, 1),
                  color: tile.value == null
                      ? MotoColors.faint
                      : (tile.value! / tile.max > 0.85
                            ? MotoColors.amber
                            : MotoColors.text),
                ),
              ),
          ],
        );
      },
    );
  }
}

class _GpsPanel extends StatelessWidget {
  const _GpsPanel({required this.gps});

  final GpsService gps;

  @override
  Widget build(BuildContext context) {
    return Panel(
      child: Row(
        children: [
          const Icon(Icons.near_me, size: 18, color: MotoColors.cyan),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  gps.hasFix ? 'GPS LOCK' : 'NO GPS FIX',
                  style: labelStyle(size: 9),
                ),
                const SizedBox(height: 3),
                Text(
                  gps.hasFix
                      ? '${gps.position!.latitude.toStringAsFixed(5)}, ${gps.position!.longitude.toStringAsFixed(5)}  ·  ${gps.heading.toStringAsFixed(0)}°'
                      : 'Enable location to feed speed and the nav view.',
                  style: TextStyle(
                    fontSize: 11.5,
                    color: gps.hasFix ? MotoColors.text : MotoColors.muted,
                  ),
                ),
              ],
            ),
          ),
          Text(gps.tripKm.toStringAsFixed(2), style: monoStyle(size: 18)),
          const SizedBox(width: 4),
          Text('km', style: labelStyle(size: 8, color: MotoColors.faint)),
        ],
      ),
    );
  }
}

class _Footer extends StatelessWidget {
  const _Footer({required this.demo, required this.values, required this.gps});

  final bool demo;
  final Map<String, double> values;
  final GpsService gps;

  @override
  Widget build(BuildContext context) {
    final protocol = ProtocolService.instance;
    final channelCount = values.length;
    return Row(
      children: [
        Icon(
          demo ? Icons.science_outlined : Icons.memory,
          size: 13,
          color: MotoColors.faint,
        ),
        const SizedBox(width: 6),
        Expanded(
          child: Text(
            demo
                ? 'Preview · $channelCount channels · no transport'
                : protocol.connected
                ? 'TCP ${protocol.host}:${protocol.port} · $channelCount channels'
                : 'No ECU link · ${protocol.message ?? 'connect in Settings'}',
            style: const TextStyle(fontSize: 10, color: MotoColors.faint),
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ),
        Text(
          '${gps.trail.length} pts',
          style: monoStyle(size: 9, color: MotoColors.faint),
        ),
      ],
    );
  }
}
