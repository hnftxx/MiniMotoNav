import 'package:flutter/material.dart';

import '../app/theme.dart';
import '../protocol_service.dart';
import '../widgets/common.dart';

class DiagnosticsScreen extends StatelessWidget {
  const DiagnosticsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final protocol = ProtocolService.instance;
    return AnimatedBuilder(
      animation: protocol,
      builder: (context, _) {
        final values = protocol.values;
        return SafeArea(
          bottom: false,
          child: ListView(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 28),
            children: [
              PageHeader(
                title: 'ECU State',
                subtitle: 'Identity, link health and service functions',
                trailing: StatusPill(
                  label: protocol.state,
                  color: protocol.connected
                      ? MotoColors.lime
                      : MotoColors.faint,
                ),
              ),
              Panel(
                child: Column(
                  children: [
                    _row(
                      'Transport',
                      protocol.connected ? 'TCP connected' : 'Not connected',
                    ),
                    _row('Endpoint', '${protocol.host}:${protocol.port}'),
                    _row('Telemetry frames', '${protocol.telemetryCount}'),
                    _row(
                      'Last update',
                      protocol.updatedAt?.toIso8601String() ?? 'never',
                    ),
                    _row('Last message', protocol.message ?? '-'),
                  ],
                ),
              ),
              SectionTitle(
                'Identity reads',
                trailing: const CapabilityBadge(
                  'verified static',
                  color: MotoColors.cyan,
                ),
              ),
              Row(
                children: [
                  Expanded(
                    child: OutlinedButton(
                      onPressed: protocol.connected
                          ? protocol.requestDeviceName
                          : null,
                      child: const Text('DEVICE NAME'),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: OutlinedButton(
                      onPressed: protocol.connected
                          ? () => protocol.requestIdentity(2)
                          : null,
                      child: const Text('IDENTITY'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              const InfoBanner(
                text: 'Identity replies are requested over the verified SDO path. This build surfaces the raw reply status only; it does not claim a decoded ECU model.',
                color: MotoColors.cyan,
              ),
              SectionTitle(
                'Live snapshot',
                trailing: Text(
                  '${values.length} channels',
                  style: labelStyle(size: 9),
                ),
              ),
              if (values.isEmpty)
                const Panel(
                  child: EmptyState(
                    icon: Icons.sensors_off,
                    title: 'No telemetry',
                    body: 'Connect to the ECU Wi-Fi transport and subscribe in Settings.',
                  ),
                )
              else
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    for (final entry in values.entries)
                      SizedBox(
                        width: 150,
                        child: MetricTile(
                          label: entry.key,
                          value: entry.value.toStringAsFixed(2),
                        ),
                      ),
                  ],
                ),
              SectionTitle(
                'ECU flags',
                trailing: const CapabilityBadge(
                  'unknown mapping',
                  color: MotoColors.amber,
                ),
              ),
              Panel(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'ECU_FLAGS_A / ECU_FLAGS_B (0x200A / 0x200B) are readable objects, but the bit meanings are not established. Bits are shown as raw when a read is wired.',
                      style: TextStyle(
                        fontSize: 11.5,
                        height: 1.45,
                        color: MotoColors.muted,
                      ),
                    ),
                    const SizedBox(height: 8),
                    Row(
                      children: [
                        const CapabilityBadge(
                          '0x200A',
                          color: MotoColors.faint,
                        ),
                        const SizedBox(width: 6),
                        const CapabilityBadge(
                          '0x200B',
                          color: MotoColors.faint,
                        ),
                        const Spacer(),
                        Text(
                          'UNKNOWN',
                          style: labelStyle(size: 9, color: MotoColors.amber),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
              SectionTitle(
                'Service functions',
                trailing: const CapabilityBadge(
                  'not enabled',
                  color: MotoColors.red,
                ),
              ),
              _ServiceTile(
                title: 'TPS learn',
                body: 'Service command sequence was not traced. No frame is emitted.',
                status: 'UNKNOWN',
              ),
              _ServiceTile(
                title: 'Service reset',
                body: 'Reset command and preconditions are unknown. No frame is emitted.',
                status: 'UNKNOWN',
              ),
              _ServiceTile(
                title: 'Actuator / output test',
                body: 'Requires a verified write path. Disabled by the read-only transport.',
                status: 'BLOCKED',
              ),
              const SizedBox(height: 10),
              const InfoBanner(
                text: 'Safety: the Kotlin transport rejects persistent writes. Nothing on this screen can change ECU state.',
                icon: Icons.lock_outline,
                color: MotoColors.lime,
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _row(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 5),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 118,
            child: Text(label.toUpperCase(), style: labelStyle(size: 9)),
          ),
          Expanded(
            child: Text(
              value,
              style: const TextStyle(fontSize: 12, height: 1.35),
            ),
          ),
        ],
      ),
    );
  }
}

class _ServiceTile extends StatelessWidget {
  const _ServiceTile({
    required this.title,
    required this.body,
    required this.status,
  });

  final String title;
  final String body;
  final String status;

  @override
  Widget build(BuildContext context) {
    final color = status == 'BLOCKED' ? MotoColors.red : MotoColors.amber;
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Panel(
        color: MotoColors.panel.withValues(alpha: 0.6),
        child: Row(
          children: [
            const Icon(Icons.lock_outline, size: 17, color: MotoColors.faint),
            const SizedBox(width: 10),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    title,
                    style: const TextStyle(
                      fontSize: 13,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 2),
                  Text(
                    body,
                    style: const TextStyle(
                      fontSize: 10.5,
                      color: MotoColors.muted,
                      height: 1.35,
                    ),
                  ),
                ],
              ),
            ),
            CapabilityBadge(status, color: color),
          ],
        ),
      ),
    );
  }
}

class MalfunctionScreen extends StatelessWidget {
  const MalfunctionScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      bottom: false,
      child: ListView(
        padding: const EdgeInsets.fromLTRB(16, 0, 16, 28),
        children: [
          const PageHeader(
            title: 'Malfunction Codes',
            subtitle: 'DTC read and clear',
          ),
          const InfoBanner(
            text: 'DTC service frames are not traced in aRacer X Tune 3.7.2. This screen never fabricates codes or sends clear requests.',
            color: MotoColors.amber,
            icon: Icons.report_gmailerrorred,
          ),
          const SizedBox(height: 14),
          Panel(
            child: Column(
              children: [
                const Icon(
                  Icons.check_circle_outline,
                  size: 38,
                  color: MotoColors.faint,
                ),
                const SizedBox(height: 10),
                const Text(
                  'No DTC data source',
                  style: TextStyle(fontSize: 15, fontWeight: FontWeight.w600),
                ),
                const SizedBox(height: 4),
                const Text(
                  'Reading and clearing fault codes requires the diagnostic object mapping, which remains UNKNOWN.',
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontSize: 11.5,
                    color: MotoColors.muted,
                    height: 1.45,
                  ),
                ),
                const SizedBox(height: 12),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    OutlinedButton(
                      onPressed: null,
                      child: const Text('READ CODES'),
                    ),
                    const SizedBox(width: 8),
                    OutlinedButton(
                      onPressed: null,
                      child: const Text('CLEAR CODES'),
                    ),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          const CapabilityBadge('status: unknown', color: MotoColors.amber),
        ],
      ),
    );
  }
}
