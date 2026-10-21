import 'package:flutter/material.dart';

import '../gps_service.dart';
import '../protocol_service.dart';
import '../widgets/arc_gauge.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  @override
  void initState() {
    super.initState();
    GpsService.instance.start();
  }

  @override
  Widget build(BuildContext context) {
    final link = ProtocolService.instance;
    final gps = GpsService.instance;
    return SafeArea(
      child: AnimatedBuilder(
        animation: Listenable.merge([link, gps]),
        builder: (context, _) {
          final rpm = link.value('RPM');
          return Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                _StatusBar(link: link, gps: gps),
                const SizedBox(height: 8),
                Expanded(
                  child: LayoutBuilder(
                    builder: (context, constraints) {
                      final wide = constraints.maxWidth > 560;
                      final gaugeSize = wide
                          ? 320.0
                          : constraints.maxWidth * 0.72;
                      return SingleChildScrollView(
                        child: Column(
                          children: [
                            ArcGauge(
                              value: rpm,
                              max: 14000,
                              label: 'RPM',
                              unit: '',
                              redline: 12000,
                              size: gaugeSize,
                            ),
                            const SizedBox(height: 4),
                            Row(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Expanded(
                                  child: _SpeedCard(
                                    kph: gps.speedKph,
                                    fix: gps.hasFix,
                                  ),
                                ),
                                const SizedBox(width: 12),
                                Expanded(
                                  child: _MetricCard(
                                    title: 'COOLANT',
                                    value: link
                                        .value('T_Eng')
                                        .toStringAsFixed(0),
                                    unit: 'degC',
                                    accent: const Color(0xFF4DD0E1),
                                  ),
                                ),
                              ],
                            ),
                            const SizedBox(height: 12),
                            GridView.count(
                              crossAxisCount: wide ? 4 : 2,
                              shrinkWrap: true,
                              physics: const NeverScrollableScrollPhysics(),
                              mainAxisSpacing: 10,
                              crossAxisSpacing: 10,
                              childAspectRatio: 1.9,
                              children: [
                                _MetricCard(
                                  title: 'TPS',
                                  value: link
                                      .value('TPS_Percent')
                                      .toStringAsFixed(1),
                                  unit: '%',
                                  accent: const Color(0xFFFFB300),
                                ),
                                _MetricCard(
                                  title: 'BATTERY',
                                  value: link
                                      .value('Volt_Batt')
                                      .toStringAsFixed(2),
                                  unit: 'V',
                                  accent: const Color(0xFF9CCC65),
                                ),
                                _MetricCard(
                                  title: 'AFR TARGET',
                                  value: link
                                      .value('AFR_Target')
                                      .toStringAsFixed(1),
                                  unit: 'AFR',
                                  accent: const Color(0xFFBA68C8),
                                ),
                                _MetricCard(
                                  title: 'IGN ANGLE',
                                  value: link.value('SA').toStringAsFixed(1),
                                  unit: 'deg',
                                  accent: const Color(0xFF64B5F6),
                                ),
                                _MetricCard(
                                  title: 'TRIP',
                                  value: gps.tripKm.toStringAsFixed(2),
                                  unit: 'km',
                                  accent: const Color(0xFFFF8A65),
                                ),
                                _MetricCard(
                                  title: 'ALTITUDE',
                                  value: (gps.position?.altitude ?? 0)
                                      .toStringAsFixed(0),
                                  unit: 'm',
                                  accent: const Color(0xFF90A4AE),
                                ),
                              ],
                            ),
                            if (!link.connected)
                              Padding(
                                padding: const EdgeInsets.only(top: 12),
                                child: TextButton.icon(
                                  onPressed: () => link.connect(),
                                  icon: const Icon(Icons.link),
                                  label: const Text('CONNECT ECU'),
                                ),
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
      ),
    );
  }
}

class _StatusBar extends StatelessWidget {
  const _StatusBar({required this.link, required this.gps});

  final ProtocolService link;
  final GpsService gps;

  @override
  Widget build(BuildContext context) {
    final color = switch (link.state) {
      'connected' => const Color(0xFF66BB6A),
      'connecting' => const Color(0xFFFFB300),
      'error' => const Color(0xFFEF5350),
      _ => Colors.white38,
    };
    return Row(
      children: [
        Container(
          width: 8,
          height: 8,
          decoration: BoxDecoration(color: color, shape: BoxShape.circle),
        ),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            link.connected
                ? 'ECU ${link.host}'
                : 'ECU ${link.state.toUpperCase()}',
            style: const TextStyle(
              fontSize: 12,
              letterSpacing: 1.2,
              color: Colors.white70,
            ),
          ),
        ),
        Icon(
          gps.hasFix ? Icons.satellite_alt : Icons.satellite_alt_outlined,
          size: 16,
          color: gps.hasFix ? const Color(0xFF4DD0E1) : Colors.white38,
        ),
        const SizedBox(width: 6),
        Text(
          gps.status.toUpperCase(),
          style: TextStyle(
            fontSize: 11,
            letterSpacing: 1.1,
            color: gps.hasFix ? const Color(0xFF4DD0E1) : Colors.white38,
          ),
        ),
      ],
    );
  }
}

class _SpeedCard extends StatelessWidget {
  const _SpeedCard({required this.kph, required this.fix});

  final double kph;
  final bool fix;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      decoration: BoxDecoration(
        color: const Color(0xFF141A21),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0x22FFFFFF)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'GPS SPEED',
            style: TextStyle(
              fontSize: 10,
              letterSpacing: 1.4,
              color: Colors.white54,
            ),
          ),
          const SizedBox(height: 6),
          Row(
            crossAxisAlignment: CrossAxisAlignment.baseline,
            textBaseline: TextBaseline.alphabetic,
            children: [
              Text(
                fix ? kph.toStringAsFixed(0) : '--',
                style: const TextStyle(
                  fontSize: 44,
                  fontWeight: FontWeight.w700,
                  height: 1,
                ),
              ),
              const SizedBox(width: 6),
              const Text(
                'km/h',
                style: TextStyle(fontSize: 12, color: Colors.white54),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _MetricCard extends StatelessWidget {
  const _MetricCard({
    required this.title,
    required this.value,
    required this.unit,
    required this.accent,
  });

  final String title;
  final String value;
  final String unit;
  final Color accent;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: const Color(0xFF141A21),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: accent.withValues(alpha: 0.22)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Text(
            title,
            style: TextStyle(
              fontSize: 9,
              letterSpacing: 1.3,
              color: accent.withValues(alpha: 0.8),
            ),
          ),
          const SizedBox(height: 5),
          Row(
            crossAxisAlignment: CrossAxisAlignment.baseline,
            textBaseline: TextBaseline.alphabetic,
            children: [
              Flexible(
                child: FittedBox(
                  fit: BoxFit.scaleDown,
                  alignment: Alignment.centerLeft,
                  child: Text(
                    value,
                    style: const TextStyle(
                      fontSize: 24,
                      fontWeight: FontWeight.w600,
                      height: 1,
                    ),
                  ),
                ),
              ),
              const SizedBox(width: 4),
              Text(
                unit,
                style: const TextStyle(fontSize: 10, color: Colors.white38),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
