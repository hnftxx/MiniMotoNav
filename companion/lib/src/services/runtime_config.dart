import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// Runtime configuration shared by Dart and the Android host.
///
/// The Maps key is read from the native BuildConfig (populated from the
/// `GOOGLE_MAPS_API_KEY` Gradle property or environment variable). A
/// `MAPS_API_KEY` Dart define is used as a fallback on desktop/test hosts.
class RuntimeConfig extends ChangeNotifier {
  RuntimeConfig._();

  static final RuntimeConfig instance = RuntimeConfig._();

  static const _channel = MethodChannel('moto/config');
  static const _dartKey = String.fromEnvironment('MAPS_API_KEY');

  String _mapsApiKey = _dartKey.trim();
  bool _loaded = false;

  bool get loaded => _loaded;
  String get mapsApiKey => _mapsApiKey;
  bool get hasMapsKey => _mapsApiKey.isNotEmpty;

  Future<void> load() async {
    if (_loaded) return;
    if (!kIsWeb && defaultTargetPlatform == TargetPlatform.android) {
      try {
        final result = await _channel.invokeMapMethod<String, dynamic>('maps');
        final key = (result?['apiKey'] as String?)?.trim() ?? '';
        if (key.isNotEmpty) _mapsApiKey = key;
      } on MissingPluginException {
        // Test hosts and non-Android builds have no native config bridge.
      } on PlatformException {
        // Keep the Dart-define fallback.
      }
    }
    _loaded = true;
    notifyListeners();
  }
}
