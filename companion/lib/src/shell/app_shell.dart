import 'package:flutter/material.dart';

import '../app/theme.dart';
import '../ble/ble_sync_service.dart';
import '../gps_service.dart';
import '../protocol_service.dart';
import '../screens/calibration_screen.dart';
import '../screens/data_screens.dart';
import '../screens/diagnostics_screen.dart';
import '../screens/monitor_screen.dart';
import '../screens/nav_screen.dart';
import '../screens/settings_screen.dart';
import '../services/app_settings.dart';
import '../telemetry/telemetry_service.dart';
import '../widgets/common.dart';

enum AppPage {
  simple,
  race,
  cube,
  navigation,
  calibration,
  powerTest,
  malfunction,
  logs,
  mapStore,
  diagnostics,
  settings,
}

extension AppPageInfo on AppPage {
  String get title => switch (this) {
    AppPage.simple => 'Simple Monitor',
    AppPage.race => 'Race Monitor',
    AppPage.cube => 'Cube Monitor',
    AppPage.navigation => 'Navigation',
    AppPage.calibration => 'Calibration',
    AppPage.powerTest => 'Power Test',
    AppPage.malfunction => 'Malfunction Codes',
    AppPage.logs => 'Log View',
    AppPage.mapStore => 'Map Store',
    AppPage.diagnostics => 'ECU State',
    AppPage.settings => 'Settings',
  };

  IconData get icon => switch (this) {
    AppPage.simple => Icons.speed,
    AppPage.race => Icons.sports_motorsports,
    AppPage.cube => Icons.grid_view_rounded,
    AppPage.navigation => Icons.navigation_outlined,
    AppPage.calibration => Icons.tune,
    AppPage.powerTest => Icons.timer_outlined,
    AppPage.malfunction => Icons.warning_amber_rounded,
    AppPage.logs => Icons.multiline_chart,
    AppPage.mapStore => Icons.map_outlined,
    AppPage.diagnostics => Icons.memory,
    AppPage.settings => Icons.settings_outlined,
  };
}

class AppShell extends StatefulWidget {
  const AppShell({super.key});

  @override
  State<AppShell> createState() => _AppShellState();
}

class _AppShellState extends State<AppShell> {
  final _scaffold = GlobalKey<ScaffoldState>();
  final _settings = AppSettings.instance;
  AppPage _page = AppPage.simple;

  @override
  void initState() {
    super.initState();
    _settings.addListener(_syncPreview);
    _syncPreview();
    TelemetryService.instance.start();
    GpsService.instance.start();
    BleSyncService.instance.start();
    switch (_settings.lastPage) {
      case 'race':
        _page = AppPage.race;
      case 'cube':
        _page = AppPage.cube;
      case 'navigation':
        _page = AppPage.navigation;
    }
  }

  @override
  void dispose() {
    _settings.removeListener(_syncPreview);
    super.dispose();
  }

  void _syncPreview() {
    TelemetryService.instance.setPreview(_settings.preview);
  }

  void _select(AppPage page) {
    setState(() => _page = page);
    _settings.setLastPage(switch (page) {
      AppPage.simple => 'simple',
      AppPage.race => 'race',
      AppPage.cube => 'cube',
      AppPage.navigation => 'navigation',
      _ => _settings.lastPage,
    });
    _scaffold.currentState?.closeDrawer();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      key: _scaffold,
      appBar: _buildAppBar(),
      drawer: _buildDrawer(),
      body: IndexedStack(
        index: _page.index,
        children: const [
          MonitorScreen(style: MonitorStyle.simple),
          MonitorScreen(style: MonitorStyle.race),
          MonitorScreen(style: MonitorStyle.cube),
          NavScreen(),
          CalibrationScreen(),
          PowerTestScreen(),
          MalfunctionScreen(),
          LogScreen(),
          MapStoreScreen(),
          DiagnosticsScreen(),
          SettingsScreen(),
        ],
      ),
    );
  }

  PreferredSizeWidget _buildAppBar() {
    return AppBar(
      toolbarHeight: 62,
      leading: Builder(
        builder: (context) => IconButton(
          onPressed: Scaffold.of(context).openDrawer,
          icon: const Icon(Icons.menu, size: 22),
          color: MotoColors.text,
        ),
      ),
      titleSpacing: 0,
      title: AnimatedBuilder(
        animation: ProtocolService.instance,
        builder: (context, _) {
          final protocol = ProtocolService.instance;
          return Row(
            children: [
              Container(
                width: 3,
                height: 26,
                decoration: BoxDecoration(
                  gradient: const LinearGradient(
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                    colors: [MotoColors.cyan, MotoColors.blue],
                  ),
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
              const SizedBox(width: 9),
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    'GODSPEED',
                    style: TextStyle(
                      fontSize: 15,
                      fontWeight: FontWeight.w800,
                      letterSpacing: 1.6,
                    ),
                  ),
                  Text(
                    _page.title.toUpperCase(),
                    style: const TextStyle(
                      fontSize: 8,
                      letterSpacing: 1.8,
                      color: MotoColors.muted,
                    ),
                  ),
                ],
              ),
              const Spacer(),
              Padding(
                padding: const EdgeInsets.only(right: 12),
                child: StatusPill(
                  label: protocol.connected ? 'linked' : 'offline',
                  color: protocol.connected
                      ? MotoColors.lime
                      : MotoColors.faint,
                ),
              ),
            ],
          );
        },
      ),
      actions: [
        AnimatedBuilder(
          animation: _settings,
          builder: (context, _) => IconButton(
            tooltip: _settings.preview ? 'Disable preview' : 'Enable preview',
            onPressed: () => _settings.setPreview(!_settings.preview),
            icon: Icon(
              _settings.preview ? Icons.science : Icons.science_outlined,
              size: 20,
              color: _settings.preview ? MotoColors.amber : MotoColors.muted,
            ),
          ),
        ),
        const SizedBox(width: 4),
      ],
    );
  }

  Widget _buildDrawer() {
    return Drawer(
      child: SafeArea(
        child: ListView(
          padding: const EdgeInsets.fromLTRB(14, 16, 14, 20),
          children: [
            _DrawerHeader(settings: _settings),
            const SizedBox(height: 16),
            _groupLabel('DASHBOARDS'),
            _navTile(AppPage.simple, 'LITE'),
            _navTile(AppPage.race, 'PRO'),
            _navTile(AppPage.cube, 'QUICK'),
            _groupLabel('WORKSPACE'),
            _navTile(AppPage.navigation),
            _navTile(AppPage.calibration),
            _navTile(AppPage.powerTest),
            _navTile(AppPage.malfunction),
            _navTile(AppPage.logs),
            _navTile(AppPage.mapStore),
            _groupLabel('SYSTEM'),
            _navTile(AppPage.diagnostics),
            _navTile(AppPage.settings),
            const SizedBox(height: 16),
            const Divider(),
            const SizedBox(height: 12),
            const Text(
              'READ-ONLY ECU LINK',
              style: TextStyle(
                fontSize: 9,
                letterSpacing: 1.6,
                color: MotoColors.faint,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 6),
            const Text(
              'Tuning writes and flashing are deliberately absent.',
              style: TextStyle(
                fontSize: 10.5,
                color: MotoColors.muted,
                height: 1.4,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _groupLabel(String text) => Padding(
    padding: const EdgeInsets.fromLTRB(8, 16, 8, 6),
    child: Text(
      text,
      style: labelStyle(size: 9, color: MotoColors.faint, spacing: 1.8),
    ),
  );

  Widget _navTile(AppPage page, [String? tag]) {
    final selected = _page == page;
    return Padding(
      padding: const EdgeInsets.only(bottom: 2),
      child: Material(
        color: selected
            ? MotoColors.cyan.withValues(alpha: 0.1)
            : Colors.transparent,
        borderRadius: BorderRadius.circular(10),
        child: InkWell(
          borderRadius: BorderRadius.circular(10),
          onTap: () => _select(page),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 10),
            child: Row(
              children: [
                Icon(
                  page.icon,
                  size: 18,
                  color: selected ? MotoColors.cyan : MotoColors.muted,
                ),
                const SizedBox(width: 11),
                Expanded(
                  child: Text(
                    page.title,
                    style: TextStyle(
                      fontSize: 13,
                      fontWeight: selected ? FontWeight.w700 : FontWeight.w500,
                      color: selected ? MotoColors.text : MotoColors.muted,
                    ),
                  ),
                ),
                if (tag != null)
                  CapabilityBadge(
                    tag,
                    color: selected ? MotoColors.cyan : MotoColors.faint,
                  )
                else if (selected)
                  Container(
                    width: 5,
                    height: 5,
                    decoration: const BoxDecoration(
                      color: MotoColors.cyan,
                      shape: BoxShape.circle,
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _DrawerHeader extends StatelessWidget {
  const _DrawerHeader({required this.settings});

  final AppSettings settings;

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: ProtocolService.instance,
      builder: (context, _) {
        final protocol = ProtocolService.instance;
        return Container(
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            gradient: const LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: [Color(0xFF11212B), Color(0xFF0C1218)],
            ),
            borderRadius: BorderRadius.circular(16),
            border: Border.all(color: MotoColors.stroke),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Container(
                    width: 38,
                    height: 38,
                    decoration: BoxDecoration(
                      color: MotoColors.cyan.withValues(alpha: 0.12),
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: MotoColors.cyan.withValues(alpha: 0.35),
                      ),
                    ),
                    child: const Icon(
                      Icons.bolt,
                      color: MotoColors.cyan,
                      size: 22,
                    ),
                  ),
                  const SizedBox(width: 10),
                  const Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'GODSPEED',
                        style: TextStyle(
                          fontSize: 14,
                          fontWeight: FontWeight.w800,
                          letterSpacing: 1.4,
                        ),
                      ),
                      Text(
                        'X-SERIES COMPANION',
                        style: TextStyle(
                          fontSize: 8,
                          letterSpacing: 1.5,
                          color: MotoColors.muted,
                        ),
                      ),
                    ],
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  StatusPill(
                    label: protocol.connected ? 'CAN linked' : 'No ECU',
                    color: protocol.connected
                        ? MotoColors.lime
                        : MotoColors.faint,
                  ),
                  const SizedBox(width: 6),
                  StatusPill(
                    label: settings.preview ? 'Preview' : 'Live',
                    color: settings.preview
                        ? MotoColors.amber
                        : MotoColors.cyan,
                  ),
                ],
              ),
            ],
          ),
        );
      },
    );
  }
}
