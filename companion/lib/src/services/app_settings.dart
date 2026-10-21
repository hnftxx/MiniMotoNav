import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

class AppSettings extends ChangeNotifier {
  AppSettings._();

  static final AppSettings instance = AppSettings._();

  SharedPreferences? _prefs;
  bool _loaded = false;

  bool metric = true;
  bool preview = false;
  String lastPage = 'simple';

  Future<void> load() async {
    if (_loaded) return;
    _prefs = await SharedPreferences.getInstance();
    metric = _prefs?.getBool('units.metric') ?? true;
    preview = _prefs?.getBool('monitor.preview') ?? false;
    lastPage = _prefs?.getString('nav.lastPage') ?? 'simple';
    _loaded = true;
    notifyListeners();
  }

  Future<void> setMetric(bool value) async {
    metric = value;
    notifyListeners();
    await _prefs?.setBool('units.metric', value);
  }

  Future<void> setPreview(bool value) async {
    preview = value;
    notifyListeners();
    await _prefs?.setBool('monitor.preview', value);
  }

  Future<void> setLastPage(String value) async {
    lastPage = value;
    await _prefs?.setString('nav.lastPage', value);
  }

  String speedUnit() => metric ? 'km/h' : 'mph';
  String temperatureUnit() => metric ? 'C' : 'F';
  String pressureUnit() => metric ? 'kPa' : 'psi';

  double speed(double kph) => metric ? kph : kph * 0.621371;
  double temperature(double celsius) => metric ? celsius : celsius * 9 / 5 + 32;
  double pressure(double kpa) => metric ? kpa : kpa * 0.145038;
}
