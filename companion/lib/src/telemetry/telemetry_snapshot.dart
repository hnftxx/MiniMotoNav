enum TelemetryField {
  rpm,
  speedKph,
  throttlePercent,
  afr,
  engineTempC,
  intakeTempC,
  batteryVolts,
  ignitionAdvanceDeg,
  gear,
}

enum TelemetrySnapshotStatus { live, stale, disconnected, error }

class TelemetrySnapshot {
  const TelemetrySnapshot({
    required this.capturedAt,
    this.rpm,
    this.speedKph,
    this.throttlePercent,
    this.afr,
    this.engineTempC,
    this.intakeTempC,
    this.batteryVolts,
    this.ignitionAdvanceDeg,
    this.gear,
    this.validFields = const {},
    this.raw = const {},
    this.status = TelemetrySnapshotStatus.live,
    this.sourceId = '',
  });

  final DateTime capturedAt;
  final double? rpm;
  final double? speedKph;
  final double? throttlePercent;
  final double? afr;
  final double? engineTempC;
  final double? intakeTempC;
  final double? batteryVolts;
  final double? ignitionAdvanceDeg;
  final double? gear;
  final Set<TelemetryField> validFields;
  final Map<String, double> raw;
  final TelemetrySnapshotStatus status;
  final String sourceId;

  bool get stale => status == TelemetrySnapshotStatus.stale;
  bool get disconnected => status == TelemetrySnapshotStatus.disconnected;
  bool get failed => status == TelemetrySnapshotStatus.error;

  bool isValid(TelemetryField field) => validFields.contains(field);

  Duration age(DateTime now) => now.difference(capturedAt);

  TelemetrySnapshot copyWith({TelemetrySnapshotStatus? status}) {
    return TelemetrySnapshot(
      capturedAt: capturedAt,
      rpm: rpm,
      speedKph: speedKph,
      throttlePercent: throttlePercent,
      afr: afr,
      engineTempC: engineTempC,
      intakeTempC: intakeTempC,
      batteryVolts: batteryVolts,
      ignitionAdvanceDeg: ignitionAdvanceDeg,
      gear: gear,
      validFields: validFields,
      raw: raw,
      status: status ?? this.status,
      sourceId: sourceId,
    );
  }

  Map<String, double> toDashboardValues() {
    return {
      'RPM': ?rpm,
      'SPEED': ?speedKph,
      'TPS_Percent': ?throttlePercent,
      'AFR_Target': ?afr,
      'T_Eng': ?engineTempC,
      'T_Air': ?intakeTempC,
      'Volt_Batt': ?batteryVolts,
      'SA': ?ignitionAdvanceDeg,
      'Gear': ?gear,
    };
  }

  static TelemetrySnapshot fromValues(
    Map<String, double> values, {
    required DateTime capturedAt,
    required String sourceId,
    TelemetrySnapshotStatus status = TelemetrySnapshotStatus.live,
  }) {
    final parsed = <TelemetryField, double>{};
    for (final entry in values.entries) {
      final field = _fieldForName(entry.key);
      if (field == null) continue;
      final value = entry.value;
      if (!value.isFinite || !_inRange(field, value)) continue;
      parsed[field] = value;
    }
    return TelemetrySnapshot(
      capturedAt: capturedAt,
      rpm: parsed[TelemetryField.rpm],
      speedKph: parsed[TelemetryField.speedKph],
      throttlePercent: parsed[TelemetryField.throttlePercent],
      afr: parsed[TelemetryField.afr],
      engineTempC: parsed[TelemetryField.engineTempC],
      intakeTempC: parsed[TelemetryField.intakeTempC],
      batteryVolts: parsed[TelemetryField.batteryVolts],
      ignitionAdvanceDeg: parsed[TelemetryField.ignitionAdvanceDeg],
      gear: parsed[TelemetryField.gear],
      validFields: parsed.keys.toSet(),
      raw: Map.unmodifiable(values),
      status: status,
      sourceId: sourceId,
    );
  }

  static TelemetryField? _fieldForName(String name) {
    return switch (name.toLowerCase()) {
      'rpm' => TelemetryField.rpm,
      'speed' || 'speed_kph' || 'speedkph' => TelemetryField.speedKph,
      'tps' || 'tps_percent' || 'throttle' => TelemetryField.throttlePercent,
      'afr' || 'afr_target' => TelemetryField.afr,
      't_eng' || 'ect' || 'engine_temp' => TelemetryField.engineTempC,
      't_air' || 'iat' || 'intake_temp' => TelemetryField.intakeTempC,
      'volt_batt' ||
      'battery' ||
      'battery_volts' => TelemetryField.batteryVolts,
      'sa' || 'ignition_advance' => TelemetryField.ignitionAdvanceDeg,
      'gear' => TelemetryField.gear,
      _ => null,
    };
  }

  static bool _inRange(TelemetryField field, double value) {
    return switch (field) {
      TelemetryField.rpm => value >= 0 && value <= 20000,
      TelemetryField.speedKph => value >= 0 && value <= 400,
      TelemetryField.throttlePercent => value >= 0 && value <= 100,
      TelemetryField.afr => value >= 5 && value <= 25,
      TelemetryField.engineTempC => value >= -60 && value <= 300,
      TelemetryField.intakeTempC => value >= -60 && value <= 300,
      TelemetryField.batteryVolts => value >= 0 && value <= 30,
      TelemetryField.ignitionAdvanceDeg => value >= -60 && value <= 80,
      TelemetryField.gear => value >= 0 && value <= 8,
    };
  }
}
