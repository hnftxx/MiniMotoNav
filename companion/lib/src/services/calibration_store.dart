import 'dart:convert';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

class CalibrationItem {
  const CalibrationItem({
    required this.absid,
    required this.name,
    required this.kind,
    required this.group,
    required this.width,
    required this.min,
    required this.max,
    required this.divisor,
    required this.offset,
    required this.unit,
    required this.access,
  });

  final int absid;
  final String name;
  final String kind;
  final String group;
  final int width;
  final double min;
  final double max;
  final double divisor;
  final double offset;
  final String unit;
  final String access;

  bool get isTable => kind == 'D3';
  double get defaultValue {
    if (!min.isFinite || !max.isFinite || max <= min) return 0;
    return min + (max - min) * 0.5;
  }

  String get rangeLabel {
    if (isTable) return '16 x 16 table';
    final lo = min.isFinite ? min.toStringAsFixed(2) : '-';
    final hi = max.isFinite ? max.toStringAsFixed(2) : '-';
    return '$lo .. $hi${unit == '_' || unit.isEmpty ? '' : ' $unit'}';
  }

  factory CalibrationItem.fromJson(Map<String, dynamic> json) {
    double d(String key) => (json[key] as num?)?.toDouble() ?? 0;
    return CalibrationItem(
      absid: (json['absid'] as num).toInt(),
      name: json['name'] as String? ?? 'UNNAMED',
      kind: json['kind'] as String? ?? 'D1',
      group: json['group'] as String? ?? 'Others',
      width: (json['width'] as num?)?.toInt() ?? 1,
      min: d('min'),
      max: d('max'),
      divisor: d('divisor'),
      offset: d('offset'),
      unit: json['unit'] as String? ?? '_',
      access: json['access'] as String? ?? 'R',
    );
  }
}

class CalibrationCatalog {
  CalibrationCatalog(this.items) : groups = {};

  final List<CalibrationItem> items;
  final Map<String, List<CalibrationItem>> groups;

  static const _pinned = [
    'Closed_Loop_Fuel',
    'Target_AFR',
    'Fuel_Cal',
    'Inj_Angle',
    'Idle Settings',
    'QS_Settings',
    'BackFire',
    'ETC',
    'Launch_Control',
    'Diag_Cal',
  ];

  static Future<CalibrationCatalog> load() async {
    final raw = await rootBundle.loadString('assets/calibration_catalog.json');
    final decoded = jsonDecode(raw) as Map<String, dynamic>;
    final items =
        (decoded['items'] as List)
            .map(
              (entry) =>
                  CalibrationItem.fromJson(entry as Map<String, dynamic>),
            )
            .toList()
          ..sort((a, b) => a.absid.compareTo(b.absid));
    final catalog = CalibrationCatalog(items);
    for (final item in items) {
      catalog.groups.putIfAbsent(item.group, () => []).add(item);
    }
    return catalog;
  }

  List<String> get orderedGroups {
    final rest = groups.keys.where((group) => !_pinned.contains(group)).toList()
      ..sort();
    return [..._pinned.where(groups.containsKey), ...rest];
  }

  List<CalibrationItem> search(String query) {
    final needle = query.trim().toLowerCase();
    if (needle.isEmpty) return const [];
    return items
        .where(
          (item) =>
              item.name.toLowerCase().contains(needle) ||
              item.group.toLowerCase().contains(needle) ||
              item.absid.toString() == needle,
        )
        .take(60)
        .toList();
  }
}

class CalibrationMapSlot {
  CalibrationMapSlot({
    required this.name,
    required this.savedAt,
    required this.values,
    required this.tables,
    required this.changed,
  });

  final String name;
  final DateTime savedAt;
  final Map<String, double> values;
  final Map<String, List<double>> tables;
  final int changed;

  Map<String, dynamic> toJson() => {
    'name': name,
    'savedAt': savedAt.toIso8601String(),
    'values': values,
    'tables': tables,
  };

  factory CalibrationMapSlot.fromJson(Map<String, dynamic> json) {
    final values =
        (json['values'] as Map?)?.map(
          (key, value) => MapEntry('$key', (value as num).toDouble()),
        ) ??
        {};
    final tables =
        (json['tables'] as Map?)?.map(
          (key, value) => MapEntry(
            '$key',
            (value as List).map((e) => (e as num).toDouble()).toList(),
          ),
        ) ??
        {};
    return CalibrationMapSlot(
      name: json['name'] as String? ?? 'Map',
      savedAt:
          DateTime.tryParse(json['savedAt'] as String? ?? '') ?? DateTime.now(),
      values: values,
      tables: tables,
      changed: values.length + tables.length,
    );
  }
}

class CalibrationStore extends ChangeNotifier {
  CalibrationStore._();

  static final CalibrationStore instance = CalibrationStore._();

  SharedPreferences? _prefs;
  bool loading = false;
  String? error;
  CalibrationCatalog? catalog;

  final Map<int, double> _values = {};
  final Map<int, List<double>> _tables = {};
  final List<CalibrationMapSlot> maps = [];

  int get changedCount => _values.length + _tables.length;

  Future<void> ensureLoaded() async {
    if (catalog != null || loading) return;
    loading = true;
    notifyListeners();
    try {
      catalog = await CalibrationCatalog.load();
      _prefs = await SharedPreferences.getInstance();
      final stored = _prefs?.getString('calibration.values');
      if (stored != null) _decode(jsonDecode(stored));
      final storedMaps = _prefs?.getString('calibration.maps');
      if (storedMaps != null) {
        for (final entry in jsonDecode(storedMaps) as List) {
          maps.add(CalibrationMapSlot.fromJson(entry as Map<String, dynamic>));
        }
      }
    } catch (e) {
      error = e.toString();
    } finally {
      loading = false;
      notifyListeners();
    }
  }

  double valueOf(CalibrationItem item) {
    if (item.isTable) return tableOf(item).first;
    return _values[item.absid] ?? item.defaultValue;
  }

  List<double> tableOf(CalibrationItem item) {
    final existing = _tables[item.absid];
    if (existing != null) return existing;
    final range = item.max > item.min ? item.max - item.min : 100.0;
    final table = List<double>.generate(256, (index) {
      final row = index ~/ 16;
      final col = index % 16;
      final wave =
          (math.sin(row / 15 * math.pi) * 0.55 +
              math.cos(col / 15 * math.pi) * 0.25 +
              0.5) /
          1.3;
      return double.parse(
        (item.min + range * wave.clamp(0.0, 1.0)).toStringAsFixed(3),
      );
    });
    _tables[item.absid] = table;
    return table;
  }

  void setValue(CalibrationItem item, double value) {
    final bounded = item.max > item.min
        ? value.clamp(item.min, item.max).toDouble()
        : value;
    _values[item.absid] = double.parse(bounded.toStringAsFixed(3));
    _persist();
    notifyListeners();
  }

  void setCell(CalibrationItem item, int index, double value) {
    final table = tableOf(item);
    final bounded = item.max > item.min
        ? value.clamp(item.min, item.max).toDouble()
        : value;
    table[index] = double.parse(bounded.toStringAsFixed(3));
    _persist();
    notifyListeners();
  }

  void reset(CalibrationItem item) {
    _values.remove(item.absid);
    _tables.remove(item.absid);
    _persist();
    notifyListeners();
  }

  String exportJson() => const JsonEncoder.withIndent('  ').convert({
    'schema': 'minimotonav.calibration.preview',
    'exportedAt': DateTime.now().toIso8601String(),
    'values': _values.map((key, value) => MapEntry('$key', value)),
    'tables': _tables.map((key, value) => MapEntry('$key', value)),
  });

  bool importJson(String text) {
    try {
      _decode(jsonDecode(text));
      _persist();
      notifyListeners();
      return true;
    } catch (_) {
      return false;
    }
  }

  void _decode(dynamic decoded) {
    if (decoded is! Map) return;
    final values = decoded['values'];
    if (values is Map) {
      for (final entry in values.entries) {
        final id = int.tryParse('${entry.key}');
        if (id != null && entry.value is num) {
          _values[id] = (entry.value as num).toDouble();
        }
      }
    }
    final tables = decoded['tables'];
    if (tables is Map) {
      for (final entry in tables.entries) {
        final id = int.tryParse('${entry.key}');
        final list = entry.value;
        if (id != null && list is List) {
          _tables[id] = list.map((e) => (e as num).toDouble()).toList();
        }
      }
    }
  }

  Future<void> saveMap(String name) async {
    maps.insert(
      0,
      CalibrationMapSlot(
        name: name.trim().isEmpty ? 'Untitled map' : name.trim(),
        savedAt: DateTime.now(),
        values: _values.map((key, value) => MapEntry('$key', value)),
        tables: _tables.map(
          (key, value) => MapEntry('$key', List<double>.from(value)),
        ),
        changed: changedCount,
      ),
    );
    await _persistMaps();
    notifyListeners();
  }

  Future<void> applyMap(CalibrationMapSlot slot) async {
    _values
      ..clear()
      ..addAll(
        slot.values.map((key, value) => MapEntry(int.parse(key), value)),
      );
    _tables
      ..clear()
      ..addAll(
        slot.tables.map(
          (key, value) => MapEntry(int.parse(key), List<double>.from(value)),
        ),
      );
    await _persist();
    notifyListeners();
  }

  Future<void> deleteMap(CalibrationMapSlot slot) async {
    maps.remove(slot);
    await _persistMaps();
    notifyListeners();
  }

  Future<void> _persist() async {
    await _prefs?.setString(
      'calibration.values',
      jsonEncode({
        'values': _values.map((key, value) => MapEntry('$key', value)),
        'tables': _tables.map((key, value) => MapEntry('$key', value)),
      }),
    );
  }

  Future<void> _persistMaps() async {
    await _prefs?.setString(
      'calibration.maps',
      jsonEncode(maps.map((slot) => slot.toJson()).toList()),
    );
  }
}
