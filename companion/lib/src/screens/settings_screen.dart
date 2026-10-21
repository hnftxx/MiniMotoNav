import 'package:flutter/material.dart';

import '../ble/ble_service.dart';
import '../gps_service.dart';
import '../protocol_service.dart';
import '../services/app_settings.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
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

  late final TextEditingController _host;
  late final TextEditingController _port;
  late final TextEditingController _ssid;
  late final TextEditingController _password;
  final Set<String> _selected = {...ProtocolService.dashboardParameters};
  final _settings = AppSettings.instance;

  @override
  void initState() {
    super.initState();
    _host = TextEditingController(text: ProtocolService.instance.host);
    _port = TextEditingController(text: '${ProtocolService.instance.port}');
    _ssid = TextEditingController(text: ProtocolService.instance.ssid);
    _password = TextEditingController(text: ProtocolService.instance.password);
  }

  @override
  void dispose() {
    _host.dispose();
    _port.dispose();
    _ssid.dispose();
    _password.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final protocol = ProtocolService.instance;
    final gps = GpsService.instance;
    final ble = BleService.instance;
    return AnimatedBuilder(
      animation: Listenable.merge([protocol, gps, _settings, ble]),
      builder: (context, _) {
        return SafeArea(
          bottom: false,
          child: ListView(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 28),
            children: [
              const PageHeader(
                title: 'Settings',
                subtitle: 'Link, channels, units and device integration',
              ),
              Panel(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Text('ECU LINK', style: labelStyle()),
                        const Spacer(),
                        StatusPill(
                          label: protocol.state,
                          color: protocol.connected
                              ? MotoColors.lime
                              : MotoColors.faint,
                        ),
                      ],
                    ),
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
                            ),
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          flex: 2,
                          child: TextField(
                            controller: _port,
                            keyboardType: TextInputType.number,
                            decoration: const InputDecoration(
                              labelText: 'Port',
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 10),
                    TextField(
                      controller: _ssid,
                      decoration: const InputDecoration(
                        labelText: 'ECU Wi-Fi name',
                        hintText: 'Select the Wi-Fi network used by the ECU',
                      ),
                    ),
                    const SizedBox(height: 10),
                    TextField(
                      controller: _password,
                      obscureText: true,
                      decoration: const InputDecoration(
                        labelText: 'ECU Wi-Fi password',
                        hintText: 'Enter the ECU Wi-Fi password',
                      ),
                    ),
                    const SizedBox(height: 10),
                    Row(
                      children: [
                        Expanded(
                          child: FilledButton(
                            onPressed: protocol.busy
                                ? null
                                : () async {
                                    if (protocol.connected) {
                                      await protocol.disconnect();
                                      return;
                                    }
                                    final ok = await protocol.connect(
                                      newHost: _host.text.trim(),
                                      newPort: int.tryParse(_port.text.trim()),
                                      newSsid: _ssid.text.trim(),
                                      newPassword: _password.text,
                                    );
                                    if (ok && _selected.isNotEmpty) {
                                      await protocol.subscribe(
                                        _selected.toList(),
                                      );
                                    }
                                  },
                            child: Text(
                              protocol.connected ? 'DISCONNECT' : 'CONNECT',
                            ),
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: OutlinedButton(
                            onPressed: protocol.connected
                                ? () => protocol.subscribe(_selected.toList())
                                : null,
                            child: const Text('SUBSCRIBE'),
                          ),
                        ),
                      ],
                    ),
                    if (protocol.message != null) ...[
                      const SizedBox(height: 8),
                      Text(
                        protocol.message!,
                        style: const TextStyle(
                          fontSize: 10.5,
                          color: MotoColors.muted,
                        ),
                      ),
                    ],
                  ],
                ),
              ),
              SectionTitle(
                'Channels',
                trailing: Text(
                  '${_selected.length} selected',
                  style: labelStyle(size: 9),
                ),
              ),
              Wrap(
                spacing: 7,
                runSpacing: 7,
                children: [
                  for (final name in _available)
                    FilterChip(
                      label: Text(name, style: const TextStyle(fontSize: 11)),
                      selected: _selected.contains(name),
                      onSelected: (value) => setState(() {
                        if (value) {
                          _selected.add(name);
                        } else {
                          _selected.remove(name);
                        }
                      }),
                      selectedColor: MotoColors.cyan.withValues(alpha: 0.16),
                      checkmarkColor: MotoColors.cyan,
                      side: const BorderSide(color: MotoColors.stroke),
                      backgroundColor: MotoColors.panel,
                    ),
                ],
              ),
              const SizedBox(height: 8),
              const InfoBanner(
                text: 'Only these ten verified telemetry channels are offered. Gear and vehicle speed are not ECU parameters on this profile; speed comes from GPS.',
                color: MotoColors.cyan,
              ),
              SectionTitle('Interface'),
              Panel(
                child: Column(
                  children: [
                    SwitchListTile(
                      contentPadding: EdgeInsets.zero,
                      title: const Text(
                        'Metric units',
                        style: TextStyle(fontSize: 14),
                      ),
                      subtitle: const Text(
                        'km/h, °C, kPa',
                        style: TextStyle(fontSize: 11, color: MotoColors.muted),
                      ),
                      value: _settings.metric,
                      onChanged: _settings.setMetric,
                    ),
                    const Divider(),
                    SwitchListTile(
                      contentPadding: EdgeInsets.zero,
                      title: const Text(
                        'Preview monitor data',
                        style: TextStyle(fontSize: 14),
                      ),
                      subtitle: const Text(
                        'Synthetic values for layout review, labelled in the UI',
                        style: TextStyle(fontSize: 11, color: MotoColors.muted),
                      ),
                      value: _settings.preview,
                      onChanged: _settings.setPreview,
                    ),
                  ],
                ),
              ),
              SectionTitle('Device services'),
              Panel(
                child: Column(
                  children: [
                    _serviceRow(
                      icon: Icons.gps_fixed,
                      title: 'GPS',
                      value: '${gps.status} · ${gps.trail.length} pts',
                      action: TextButton(
                        onPressed: gps.start,
                        child: const Text('RETRY'),
                      ),
                    ),
                    const Divider(),
                    _serviceRow(
                      icon: Icons.bluetooth,
                      title: 'ESP32 / BLE',
                      value: ble.ready
                          ? '${ble.name ?? 'display'} · MTU ${ble.mtu}'
                          : ble.state,
                      action: TextButton(
                        onPressed: ble.ready
                            ? ble.disconnect
                            : (ble.scanning ? ble.stopScan : ble.scan),
                        child: Text(
                          ble.ready
                              ? 'DISCONNECT'
                              : (ble.scanning ? 'STOP' : 'SCAN'),
                        ),
                      ),
                    ),
                    const Divider(),
                    _serviceRow(
                      icon: Icons.lan,
                      title: 'ECU transport',
                      value: protocol.connected
                          ? '${protocol.host}:${protocol.port}'
                          : 'Wi-Fi TCP 6666',
                      action: const CapabilityBadge(
                        'read-only',
                        color: MotoColors.lime,
                      ),
                    ),
                  ],
                ),
              ),
              if (ble.devices.isNotEmpty) ...[
                SectionTitle(
                  'Display candidates',
                  trailing: Text(
                    '${ble.devices.length} found',
                    style: labelStyle(size: 9),
                  ),
                ),
                Panel(
                  child: Column(
                    children: [
                      for (final device in ble.devices.take(6))
                        ListTile(
                          contentPadding: EdgeInsets.zero,
                          dense: true,
                          title: Text(
                            device.name,
                            style: const TextStyle(fontSize: 13),
                          ),
                          subtitle: Text(
                            '${device.address} · ${device.rssi} dBm',
                            style: const TextStyle(
                              fontSize: 10,
                              color: MotoColors.muted,
                            ),
                          ),
                          trailing: TextButton(
                            onPressed: () => ble.connect(device),
                            child: const Text('LINK'),
                          ),
                        ),
                    ],
                  ),
                ),
              ],
              if (ble.message != null)
                InfoBanner(
                  text: ble.message!,
                  color: ble.state == 'error'
                      ? MotoColors.amber
                      : MotoColors.cyan,
                ),
              SectionTitle('About'),
              const Panel(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'GODSPEED Companion',
                      style: TextStyle(
                        fontSize: 15,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                    SizedBox(height: 4),
                    Text(
                      'Protocol and feature reference: aRacer X Tune 3.7.2 static analysis. Independent UI, no aRacer branding or assets.',
                      style: TextStyle(
                        fontSize: 11.5,
                        color: MotoColors.muted,
                        height: 1.45,
                      ),
                    ),
                    SizedBox(height: 10),
                    Text(
                      'Safety: tuning writes are disabled. The transport exposes read and subscribe only.',
                      style: TextStyle(
                        fontSize: 11,
                        color: MotoColors.amber,
                        height: 1.4,
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _serviceRow({
    required IconData icon,
    required String title,
    required String value,
    required Widget action,
  }) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          Icon(icon, size: 18, color: MotoColors.cyan),
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
                Text(
                  value,
                  style: const TextStyle(
                    fontSize: 10.5,
                    color: MotoColors.muted,
                  ),
                ),
              ],
            ),
          ),
          action,
        ],
      ),
    );
  }
}
