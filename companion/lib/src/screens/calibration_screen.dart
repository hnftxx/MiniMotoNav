import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../app/theme.dart';
import '../services/calibration_store.dart';
import '../widgets/common.dart';

class CalibrationScreen extends StatefulWidget {
  const CalibrationScreen({super.key});

  @override
  State<CalibrationScreen> createState() => _CalibrationScreenState();
}

class _CalibrationScreenState extends State<CalibrationScreen> {
  final _search = TextEditingController();
  final _store = CalibrationStore.instance;
  String? _group;
  CalibrationItem? _item;

  @override
  void initState() {
    super.initState();
    _store.ensureLoaded();
    _search.addListener(() => setState(() {}));
  }

  @override
  void dispose() {
    _search.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _store,
      builder: (context, _) {
        final catalog = _store.catalog;
        return SafeArea(
          bottom: false,
          child: Column(
            children: [
              PageHeader(
                title: _item != null ? _item!.name : (_group ?? 'Calibration'),
                subtitle: _item != null
                    ? '${_item!.kind} · absid ${_item!.absid} · ${_item!.group}'
                    : 'aRacer X-Series parameter tree · ${catalog?.items.length ?? 0} tabs',
                trailing: _item != null
                    ? IconButton(
                        onPressed: () => setState(() => _item = null),
                        icon: const Icon(Icons.close, size: 20),
                        tooltip: 'Close editor',
                      )
                    : null,
              ),
              Padding(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
                child: Row(
                  children: [
                    CapabilityBadge(
                      '${_store.changedCount} local edits',
                      color: MotoColors.cyan,
                    ),
                    const SizedBox(width: 6),
                    if (_item == null)
                      const CapabilityBadge(
                        'read-only link',
                        color: MotoColors.amber,
                      ),
                    const Spacer(),
                    IconButton(
                      tooltip: 'Export calibration JSON',
                      onPressed: () => _export(context),
                      icon: const Icon(Icons.ios_share, size: 19),
                      color: MotoColors.muted,
                    ),
                    IconButton(
                      tooltip: 'Import calibration JSON',
                      onPressed: () => _import(context),
                      icon: const Icon(Icons.file_download_outlined, size: 19),
                      color: MotoColors.muted,
                    ),
                  ],
                ),
              ),
              Expanded(
                child: catalog == null
                    ? Center(
                        child: _store.error != null
                            ? Text(
                                'Catalog failed: ${_store.error}',
                                style: const TextStyle(color: MotoColors.red),
                              )
                            : const CircularProgressIndicator(strokeWidth: 2),
                      )
                    : _buildBody(catalog),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildBody(CalibrationCatalog catalog) {
    if (_item != null) return _buildEditor(_item!);

    final query = _search.text.trim();
    if (query.isNotEmpty) {
      final results = catalog.search(query);
      return Column(
        children: [
          _searchField(),
          Expanded(
            child: results.isEmpty
                ? const EmptyState(
                    icon: Icons.search_off,
                    title: 'No parameter found',
                  )
                : ListView.separated(
                    padding: const EdgeInsets.fromLTRB(16, 4, 16, 24),
                    itemCount: results.length,
                    separatorBuilder: (_, _) => const SizedBox(height: 6),
                    itemBuilder: (context, index) => _ItemTile(
                      item: results[index],
                      onTap: () => setState(() => _item = results[index]),
                    ),
                  ),
          ),
        ],
      );
    }

    if (_group != null) {
      final items = catalog.groups[_group] ?? const [];
      return Column(
        children: [
          _searchField(),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 6),
            child: Row(
              children: [
                TextButton.icon(
                  onPressed: () => setState(() => _group = null),
                  icon: const Icon(Icons.arrow_back, size: 15),
                  label: const Text('ALL GROUPS'),
                ),
                const Spacer(),
                Text('${items.length} tabs', style: labelStyle(size: 9)),
              ],
            ),
          ),
          Expanded(
            child: ListView.separated(
              padding: const EdgeInsets.fromLTRB(16, 4, 16, 24),
              itemCount: items.length,
              separatorBuilder: (_, _) => const SizedBox(height: 6),
              itemBuilder: (context, index) => _ItemTile(
                item: items[index],
                onTap: () => setState(() => _item = items[index]),
              ),
            ),
          ),
        ],
      );
    }

    return Column(
      children: [
        _searchField(),
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
          child: InfoBanner(
            text: _store.changedCount == 0
                ? 'Browse every calibration tab. D1/D3 editors change local preview values only.'
                : '${_store.changedCount} local edits stored on this device. Nothing has been sent to an ECU.',
            color: MotoColors.cyan,
          ),
        ),
        Expanded(
          child: LayoutBuilder(
            builder: (context, constraints) {
              final columns = constraints.maxWidth > 760 ? 3 : 2;
              return GridView.builder(
                padding: const EdgeInsets.fromLTRB(16, 4, 16, 24),
                gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                  crossAxisCount: columns,
                  mainAxisSpacing: 10,
                  crossAxisSpacing: 10,
                  childAspectRatio: 1.35,
                ),
                itemCount: catalog.orderedGroups.length,
                itemBuilder: (context, index) {
                  final group = catalog.orderedGroups[index];
                  final items = catalog.groups[group] ?? const [];
                  final tables = items.where((item) => item.isTable).length;
                  return _GroupCard(
                    group: group,
                    count: items.length,
                    tables: tables,
                    onTap: () => setState(() => _group = group),
                  );
                },
              );
            },
          ),
        ),
      ],
    );
  }

  Widget _searchField() {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 0, 16, 10),
      child: TextField(
        controller: _search,
        textInputAction: TextInputAction.search,
        decoration: InputDecoration(
          hintText: 'Search 1,493 tabs, groups, absid',
          prefixIcon: const Icon(Icons.search, size: 19),
          suffixIcon: _search.text.isEmpty
              ? null
              : IconButton(
                  icon: const Icon(Icons.clear, size: 17),
                  onPressed: () => _search.clear(),
                ),
        ),
      ),
    );
  }

  Widget _buildEditor(CalibrationItem item) {
    return ListView(
      padding: const EdgeInsets.fromLTRB(16, 0, 16, 28),
      children: [
        InfoBanner(
          text: _store.changedCount > 0
              ? 'Local preview edits only. ECU calibration write remains disabled in this build.'
              : 'Local preview editor. ECU read/write is not enabled, so no value shown here is a live ECU value.',
          color: MotoColors.amber,
          icon: Icons.shield_outlined,
        ),
        const SizedBox(height: 12),
        if (item.isTable)
          _TableEditor(item: item)
        else
          _ScalarEditor(item: item),
        const SizedBox(height: 12),
        Panel(
          child: Row(
            children: [
              const Icon(Icons.info_outline, size: 15, color: MotoColors.faint),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  'Storage ${item.access} · width ${item.width} B · divisor ${item.divisor.toStringAsFixed(3)} · offset ${item.offset.toStringAsFixed(3)}',
                  style: const TextStyle(
                    fontSize: 11,
                    color: MotoColors.muted,
                    height: 1.4,
                  ),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Future<void> _export(BuildContext context) async {
    await Clipboard.setData(ClipboardData(text: _store.exportJson()));
    if (!context.mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Calibration JSON copied to clipboard')),
    );
  }

  Future<void> _import(BuildContext context) async {
    final controller = TextEditingController();
    final text = await showDialog<String>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: MotoColors.panel,
        title: const Text('Import calibration JSON'),
        content: SizedBox(
          width: 460,
          child: TextField(
            controller: controller,
            maxLines: 8,
            decoration: const InputDecoration(
              hintText: 'Paste GODSPEED calibration JSON',
            ),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () async {
              final data = await Clipboard.getData(Clipboard.kTextPlain);
              if (data?.text != null) controller.text = data!.text!;
            },
            child: const Text('PASTE'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('CANCEL'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, controller.text),
            child: const Text('IMPORT'),
          ),
        ],
      ),
    );
    controller.dispose();
    if (text == null) return;
    final ok = _store.importJson(text);
    if (!context.mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(ok ? 'Calibration imported' : 'Invalid calibration JSON'),
      ),
    );
  }
}

class _GroupCard extends StatelessWidget {
  const _GroupCard({
    required this.group,
    required this.count,
    required this.tables,
    required this.onTap,
  });

  final String group;
  final int count;
  final int tables;
  final VoidCallback onTap;

  static const _icons = {
    'Closed_Loop_Fuel': Icons.water_drop_outlined,
    'Target_AFR': Icons.tune,
    'Fuel_Cal': Icons.local_gas_station_outlined,
    'Inj_Angle': Icons.schedule,
    'Idle Settings': Icons.motion_photos_paused,
    'QS_Settings': Icons.swap_vert,
    'BackFire': Icons.local_fire_department_outlined,
    'ETC': Icons.settings_input_component,
    'Launch_Control': Icons.rocket_launch_outlined,
    'Diag_Cal': Icons.monitor_heart_outlined,
    'TCS_Cal': Icons.settings_input_antenna,
    'MGU_Cal': Icons.bolt,
    'Gear_Settings': Icons.settings_outlined,
    'Warm_Up_Cal': Icons.thermostat,
  };

  @override
  Widget build(BuildContext context) {
    return Panel(
      onTap: onTap,
      padding: const EdgeInsets.all(13),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(
                _icons[group] ?? Icons.memory,
                size: 19,
                color: MotoColors.cyan,
              ),
              const Spacer(),
              if (tables > 0)
                CapabilityBadge('$tables D3', color: MotoColors.blue),
            ],
          ),
          const Spacer(),
          Text(
            group.replaceAll('_', ' '),
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              height: 1.2,
            ),
          ),
          const SizedBox(height: 4),
          Text(
            '$count tabs',
            style: labelStyle(size: 9, color: MotoColors.faint),
          ),
        ],
      ),
    );
  }
}

class _ItemTile extends StatelessWidget {
  const _ItemTile({required this.item, required this.onTap});

  final CalibrationItem item;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Panel(
      onTap: onTap,
      padding: const EdgeInsets.symmetric(horizontal: 13, vertical: 11),
      child: Row(
        children: [
          Container(
            width: 34,
            height: 34,
            alignment: Alignment.center,
            decoration: BoxDecoration(
              color: MotoColors.cyan.withValues(alpha: 0.08),
              borderRadius: BorderRadius.circular(9),
            ),
            child: Text(
              item.kind,
              style: monoStyle(size: 9, color: MotoColors.cyan),
            ),
          ),
          const SizedBox(width: 11),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  item.name,
                  style: const TextStyle(
                    fontSize: 13,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  'absid ${item.absid} · ${item.rangeLabel}',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(fontSize: 10, color: MotoColors.faint),
                ),
              ],
            ),
          ),
          const Icon(Icons.chevron_right, size: 18, color: MotoColors.faint),
        ],
      ),
    );
  }
}

class _ScalarEditor extends StatefulWidget {
  const _ScalarEditor({required this.item});

  final CalibrationItem item;

  @override
  State<_ScalarEditor> createState() => _ScalarEditorState();
}

class _ScalarEditorState extends State<_ScalarEditor> {
  late final TextEditingController _controller;
  late double _value;

  @override
  void initState() {
    super.initState();
    _value = CalibrationStore.instance.valueOf(widget.item);
    _controller = TextEditingController(text: _value.toStringAsFixed(2));
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _apply(double value) {
    setState(() => _value = value);
    _controller.text = value.toStringAsFixed(2);
    CalibrationStore.instance.setValue(widget.item, value);
  }

  @override
  Widget build(BuildContext context) {
    final item = widget.item;
    final hasRange = item.max > item.min;
    return Column(
      children: [
        Panel(
          padding: const EdgeInsets.fromLTRB(18, 16, 18, 18),
          child: Column(
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.end,
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('CURRENT VALUE', style: labelStyle()),
                        const SizedBox(height: 4),
                        Text(
                          _value.toStringAsFixed(2),
                          style: monoStyle(
                            size: 42,
                            weight: FontWeight.w700,
                            color: MotoColors.cyan,
                          ),
                        ),
                      ],
                    ),
                  ),
                  Text(
                    item.unit == '_' || item.unit.isEmpty ? '' : item.unit,
                    style: labelStyle(size: 12),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              if (hasRange)
                Slider(
                  value: _value.clamp(item.min, item.max),
                  min: item.min,
                  max: item.max,
                  onChanged: _apply,
                )
              else
                const InfoBanner(
                  text: 'This tab reports no numeric range.',
                  color: MotoColors.muted,
                ),
              Row(
                children: [
                  Text(
                    item.min.toStringAsFixed(1),
                    style: monoStyle(size: 9, color: MotoColors.faint),
                  ),
                  const Spacer(),
                  Text(
                    item.max.toStringAsFixed(1),
                    style: monoStyle(size: 9, color: MotoColors.faint),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 10),
        Row(
          children: [
            Expanded(
              child: TextField(
                controller: _controller,
                keyboardType: const TextInputType.numberWithOptions(
                  decimal: true,
                  signed: true,
                ),
                decoration: const InputDecoration(
                  labelText: 'Exact value',
                  isDense: true,
                ),
                onSubmitted: (text) {
                  final parsed = double.tryParse(text);
                  if (parsed != null) _apply(parsed.clamp(item.min, item.max));
                },
              ),
            ),
            const SizedBox(width: 8),
            OutlinedButton(
              onPressed: () {
                CalibrationStore.instance.reset(item);
                _apply(CalibrationStore.instance.valueOf(item));
              },
              child: const Text('RESET'),
            ),
          ],
        ),
        const SizedBox(height: 10),
        Row(
          children: [
            Expanded(
              child: OutlinedButton.icon(
                onPressed: null,
                icon: const Icon(Icons.upload, size: 16),
                label: const Text('SEND TO ECU'),
              ),
            ),
            const SizedBox(width: 8),
            const CapabilityBadge(
              'disabled: no verified write',
              color: MotoColors.red,
            ),
          ],
        ),
      ],
    );
  }
}

class _TableEditor extends StatelessWidget {
  const _TableEditor({required this.item});

  final CalibrationItem item;

  String _cell(double value) {
    if (value.abs() >= 100) return value.toStringAsFixed(0);
    if (value.abs() >= 10) return value.toStringAsFixed(1);
    return value.toStringAsFixed(2);
  }

  @override
  Widget build(BuildContext context) {
    final store = CalibrationStore.instance;
    final table = store.tableOf(item);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Panel(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          child: Row(
            children: [
              const Icon(Icons.grid_on, size: 16, color: MotoColors.blue),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  '16 x 16 preview surface · tap a cell to edit',
                  style: const TextStyle(
                    fontSize: 11.5,
                    color: MotoColors.muted,
                  ),
                ),
              ),
              CapabilityBadge(item.kind, color: MotoColors.blue),
            ],
          ),
        ),
        const SizedBox(height: 10),
        Container(
          decoration: BoxDecoration(
            color: MotoColors.panel,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: MotoColors.stroke),
          ),
          child: SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            padding: const EdgeInsets.all(10),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                for (var row = 0; row < 16; row++)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 3),
                    child: Row(
                      children: [
                        SizedBox(
                          width: 22,
                          child: Text(
                            '$row',
                            style: monoStyle(size: 8, color: MotoColors.faint),
                          ),
                        ),
                        for (var col = 0; col < 16; col++)
                          _cellWidget(context, store, table, row * 16 + col),
                      ],
                    ),
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 8),
        Row(
          children: [
            Text(
              'min ${item.min.toStringAsFixed(1)}',
              style: monoStyle(size: 9, color: MotoColors.faint),
            ),
            const Spacer(),
            Text(
              'max ${item.max.toStringAsFixed(1)}',
              style: monoStyle(size: 9, color: MotoColors.faint),
            ),
          ],
        ),
      ],
    );
  }

  Widget _cellWidget(
    BuildContext context,
    CalibrationStore store,
    List<double> table,
    int index,
  ) {
    final value = table[index];
    final span = item.max > item.min ? item.max - item.min : 1.0;
    final t = ((value - item.min) / span).clamp(0.0, 1.0);
    final color = Color.lerp(const Color(0xFF123A4A), MotoColors.cyan, t)!;
    return GestureDetector(
      onTap: () => _editCell(context, store, index, value),
      child: Container(
        width: 46,
        height: 30,
        margin: const EdgeInsets.only(right: 3),
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: color,
          borderRadius: BorderRadius.circular(4),
        ),
        child: Text(
          _cell(value),
          style: monoStyle(
            size: 8,
            color: t > 0.55 ? const Color(0xFF00232C) : MotoColors.text,
          ),
        ),
      ),
    );
  }

  Future<void> _editCell(
    BuildContext context,
    CalibrationStore store,
    int index,
    double value,
  ) async {
    var draft = value;
    final controller = TextEditingController(text: value.toStringAsFixed(2));
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (context) => StatefulBuilder(
        builder: (context, setSheet) => Padding(
          padding: EdgeInsets.fromLTRB(
            18,
            0,
            18,
            18 + MediaQuery.viewInsetsOf(context).bottom,
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'CELL ${index ~/ 16} : ${index % 16}',
                style: labelStyle(size: 10),
              ),
              const SizedBox(height: 6),
              Text(
                draft.toStringAsFixed(2),
                style: monoStyle(
                  size: 34,
                  weight: FontWeight.w700,
                  color: MotoColors.cyan,
                ),
              ),
              Slider(
                value: draft.clamp(item.min, item.max),
                min: item.min,
                max: item.max,
                onChanged: (next) {
                  setSheet(() => draft = next);
                  controller.text = next.toStringAsFixed(2);
                },
              ),
              TextField(
                controller: controller,
                keyboardType: const TextInputType.numberWithOptions(
                  decimal: true,
                  signed: true,
                ),
                decoration: const InputDecoration(labelText: 'Value'),
                onChanged: (text) {
                  final parsed = double.tryParse(text);
                  if (parsed != null) {
                    setSheet(() => draft = parsed.clamp(item.min, item.max));
                  }
                },
              ),
              const SizedBox(height: 12),
              SizedBox(
                width: double.infinity,
                child: FilledButton(
                  onPressed: () {
                    store.setCell(item, index, draft);
                    Navigator.pop(context);
                  },
                  child: const Text('APPLY LOCALLY'),
                ),
              ),
            ],
          ),
        ),
      ),
    );
    controller.dispose();
  }
}
