import '../navigation/navigation_state.dart';
import '../telemetry/telemetry_snapshot.dart';

class DisplayFrame {
  const DisplayFrame({
    required this.telemetry,
    required this.navigation,
    required this.ecuConnected,
    required this.telemetryAge,
  });

  final TelemetrySnapshot? telemetry;
  final NavigationState navigation;
  final bool ecuConnected;
  final Duration telemetryAge;

  bool get telemetryStale =>
      telemetry == null ||
      telemetryAge > const Duration(seconds: 2) ||
      !ecuConnected;

  Map<String, double> get values => telemetry?.toDashboardValues() ?? const {};

  factory DisplayFrame.merge({
    TelemetrySnapshot? telemetry,
    required NavigationState navigation,
    required bool ecuConnected,
    DateTime? now,
  }) {
    final clock = now ?? DateTime.now();
    return DisplayFrame(
      telemetry: telemetry,
      navigation: navigation,
      ecuConnected: ecuConnected,
      telemetryAge: telemetry == null
          ? const Duration(days: 1)
          : clock.difference(telemetry.capturedAt),
    );
  }
}
