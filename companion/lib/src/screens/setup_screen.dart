import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../protocol_service.dart';

class SetupScreen extends StatefulWidget {
  const SetupScreen({super.key});

  @override
  State<SetupScreen> createState() => _SetupScreenState();
}

class _SetupScreenState extends State<SetupScreen> {
  static const _available = [
    'RPM',
    'TPS_Percent',
    'T_Eng',
    'Volt_Batt',
    'SA',
    'AFR_Target',
    'T_Air',
    'NBO2_Volt',
    'Fuel_CL',
    'P_Atm',
  ];

  final _host = TextEditingController();
  final _port = TextEditingController();
  final Set<String> _selected = {...ProtocolService.dashboardParameters};

  @override
  void initState() {
    super.initState();
    final link = ProtocolService.instance;
    _host.text = link.host;
    _port.text = '${link.port}';
    _load();
  }

  Future<void> _load() async {
    final prefs = await SharedPreferences.getInstance();
    final host = prefs.getString('host');
    final port = prefs.getInt('port');
    final names = prefs.getStringList('parameters');
    if (!mounted) return;
    setState(() {
      if (host != null) _host.text = host;
      if (port != null) _port.text = '$port';
      if (names != null && names.isNotEmpty) {
        _selected
          ..clear()
          ..addAll(names);
      }
    });
  }

  Future<void> _save() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('host', _host.text.trim());
    await prefs.setInt('port', int.tryParse(_port.text.trim()) ?? 6666);
    await prefs.setStringList('parameters', _selected.toList()..sort());
  }

  @override
  void dispose() {
    _host.dispose();
    _port.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final link = ProtocolService.instance;
    return SafeArea(
      child: AnimatedBuilder(
        animation: link,
        builder: (context, _) {
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 16, 16, 24),
            children: [
              const Text('ECU LINK', style: _labelStyle),
              const SizedBox(height: 10),
              Row(
                children: [
                  Expanded(
                    flex: 3,
                    child: TextField(
                      controller: _host,
                      keyboardType: TextInputType.url,
                      decoration: const InputDecoration(
                        labelText: 'Host',
                        border: OutlineInputBorder(),
                        isDense: true,
                      ),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _port,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(
                        labelText: 'Port',
                        border: OutlineInputBorder(),
                        isDense: true,
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: FilledButton.icon(
                      onPressed: link.busy
                          ? null
                          : () async {
                              await _save();
                              await link.connect(
                                newHost: _host.text.trim(),
                                newPort:
                                    int.tryParse(_port.text.trim()) ?? 6666,
                              );
                              if (link.connected) {
                                await link.subscribe(_selected.toList());
                              }
                            },
                      icon: const Icon(Icons.link),
                      label: Text(link.busy ? 'CONNECTING' : 'CONNECT'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: link.connected ? link.disconnect : null,
                      icon: const Icon(Icons.link_off),
                      label: const Text('DISCONNECT'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 14),
              _InfoRow(label: 'STATE', value: link.state.toUpperCase()),
              _InfoRow(label: 'ENDPOINT', value: '${link.host}:${link.port}'),
              _InfoRow(label: 'FRAMES', value: '${link.telemetryCount}'),
              _InfoRow(
                label: 'LAST UPDATE',
                value: link.updatedAt == null
                    ? 'never'
                    : '${DateTime.now().difference(link.updatedAt!).inMilliseconds} ms ago',
              ),
              if (link.message != null)
                _InfoRow(label: 'NOTE', value: link.message!),
              const SizedBox(height: 20),
              const Text('TELEMETRY SET', style: _labelStyle),
              const SizedBox(height: 4),
              const Text(
                'Read-only SDO reads with a TPDO subscription. The app cannot write calibration or configuration objects.',
                style: TextStyle(fontSize: 11, color: Colors.white38),
              ),
              const SizedBox(height: 8),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: _available.map((name) {
                  final on = _selected.contains(name);
                  return FilterChip(
                    label: Text(name),
                    selected: on,
                    onSelected: (value) {
                      setState(() {
                        if (value) {
                          _selected.add(name);
                        } else {
                          _selected.remove(name);
                        }
                      });
                    },
                  );
                }).toList(),
              ),
              const SizedBox(height: 12),
              OutlinedButton.icon(
                onPressed: link.connected
                    ? () async {
                        await _save();
                        final count = await link.subscribe(_selected.toList());
                        if (!context.mounted) return;
                        ScaffoldMessenger.of(context).showSnackBar(
                          SnackBar(
                            content: Text('Subscribed to $count parameters'),
                          ),
                        );
                      }
                    : null,
                icon: const Icon(Icons.playlist_add_check),
                label: const Text('APPLY SUBSCRIPTION'),
              ),
              const SizedBox(height: 20),
              const Text('ECU IDENTITY', style: _labelStyle),
              const SizedBox(height: 8),
              Row(
                children: [
                  Expanded(
                    child: OutlinedButton(
                      onPressed: link.connected ? link.requestDeviceName : null,
                      child: const Text('DEVICE NAME'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: OutlinedButton(
                      onPressed: link.connected
                          ? () => link.requestIdentity(0)
                          : null,
                      child: const Text('IDENTITY'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 20),
              const Text('LIMITS', style: _labelStyle),
              const SizedBox(height: 6),
              const Text(
                'This build is read-only. Calibration writes, service resets and firmware flashing are not implemented.',
                style: TextStyle(
                  fontSize: 11,
                  color: Colors.white38,
                  height: 1.5,
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

const _labelStyle = TextStyle(
  fontSize: 10,
  letterSpacing: 1.5,
  color: Colors.white54,
);

class _InfoRow extends StatelessWidget {
  const _InfoRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        children: [
          SizedBox(
            width: 100,
            child: Text(
              label,
              style: const TextStyle(
                fontSize: 10,
                letterSpacing: 1.2,
                color: Colors.white38,
              ),
            ),
          ),
          Expanded(
            child: Text(
              value,
              style: const TextStyle(
                fontSize: 12,
                color: Colors.white70,
                fontFamily: 'monospace',
              ),
            ),
          ),
        ],
      ),
    );
  }
}
