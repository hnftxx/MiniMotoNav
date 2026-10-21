import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';

/// Synthetic monitor data for UI review only. Never presented as ECU data.
class PreviewSource extends ChangeNotifier {
  PreviewSource._();

  static final PreviewSource instance = PreviewSource._();

  Timer? _timer;
  double _phase = 0;
  Map<String, double> values = const {};

  bool get running => _timer != null;

  void start() {
    if (_timer != null) return;
    _timer = Timer.periodic(const Duration(milliseconds: 70), (_) {
      _phase += 0.055;
      final rpm = 3200 + 2600 * math.sin(_phase) + 900 * math.sin(_phase * 2.3);
      values = {
        'RPM': rpm.clamp(900, 15800).toDouble(),
        'TPS_Percent': (38 + 34 * math.sin(_phase * 0.8))
            .clamp(0, 100)
            .toDouble(),
        'T_Eng': 88 + 6 * math.sin(_phase * 0.17),
        'Volt_Batt': 14.2 + 0.15 * math.sin(_phase * 0.5),
        'SA': 26 + 12 * math.sin(_phase * 0.55),
        'AFR_Target': 13.1 + 0.7 * math.sin(_phase * 0.31),
        'T_Air': 31 + 2 * math.sin(_phase * 0.12),
        'P_Atm': 101.3 + 0.4 * math.sin(_phase * 0.09),
      };
      notifyListeners();
    });
  }

  void stop() {
    _timer?.cancel();
    _timer = null;
    values = const {};
    notifyListeners();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }
}
