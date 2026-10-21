import 'navigation_state.dart';

/// Guidance parsed from a Google Maps navigation notification.
///
/// The listener forwards the raw notification fields; parsing stays here so it
/// can be unit-tested without an Android device.
class CompanionGuidance {
  const CompanionGuidance({
    required this.maneuver,
    required this.distanceToManeuverM,
    required this.roadName,
    required this.eta,
    required this.arrived,
    required this.postedAt,
    required this.key,
  });

  final NavigationManeuver maneuver;
  final double distanceToManeuverM;
  final String roadName;
  final DateTime? eta;
  final bool arrived;
  final DateTime postedAt;
  final String key;
}

class NotificationNavParser {
  static const _distancePattern = r'(\d+(?:[.,]\d+)?)\s*(km|m|mi|ft)\b';
  static const _etaPattern = r'(?:eta|arrive|arrival)[^\d]{0,8}(\d{1,2}):(\d{2})';
  static const _minutesPattern = r'(\d+)\s*min\b';

  /// Returns null when the notification carries no usable navigation content.
  static CompanionGuidance? parse(
    Map<Object?, Object?> raw, {
    DateTime? now,
  }) {
    final title = _text(raw['title']);
    final text = _text(raw['text']);
    final bigText = _text(raw['bigText']);
    final subText = _text(raw['subText']);
    final combined = [title, text, bigText, subText]
        .where((part) => part.isNotEmpty)
        .join(' · ');
    if (combined.isEmpty) return null;

    final postedSeconds = (raw['postTime'] as num?)?.toInt();
    final postedAt = postedSeconds == null
        ? (now ?? DateTime.now())
        : DateTime.fromMillisecondsSinceEpoch(postedSeconds);
    final key = _text(raw['key']);
    final lower = combined.toLowerCase();
    final arrived =
        lower.contains('you have arrived') ||
        lower.contains('reached your destination') ||
        lower.contains('arrived at');

    final maneuver = arrived
        ? NavigationManeuver.arrive
        : _maneuver(lower);
    final distance = _distance(combined);
    final roadName = _roadName(title, text, bigText);
    final eta = _eta(combined, postedAt);

    if (!arrived &&
        maneuver == NavigationManeuver.none &&
        distance == null &&
        eta == null) {
      return null;
    }
    return CompanionGuidance(
      maneuver: maneuver,
      distanceToManeuverM: arrived ? 0 : (distance ?? 0),
      roadName: roadName,
      eta: eta,
      arrived: arrived,
      postedAt: postedAt,
      key: key,
    );
  }

  static String _text(Object? value) =>
      value is String ? value.trim() : '';

  static NavigationManeuver _maneuver(String text) {
    if (text.contains('u-turn') || text.contains('u turn')) {
      return NavigationManeuver.uTurn;
    }
    if (text.contains('roundabout') ||
        text.contains('take the') && text.contains('exit') ||
        text.contains('rotary')) {
      return NavigationManeuver.roundabout;
    }
    if (text.contains('sharp left')) return NavigationManeuver.sharpLeft;
    if (text.contains('sharp right')) return NavigationManeuver.sharpRight;
    if (text.contains('slight left') || text.contains('keep left')) {
      return NavigationManeuver.slightLeft;
    }
    if (text.contains('slight right') || text.contains('keep right')) {
      return NavigationManeuver.slightRight;
    }
    if (text.contains('turn left') || text.contains('left onto')) {
      return NavigationManeuver.left;
    }
    if (text.contains('turn right') || text.contains('right onto')) {
      return NavigationManeuver.right;
    }
    if (text.contains('straight') ||
        text.contains('continue') ||
        text.contains('head ')) {
      return NavigationManeuver.straight;
    }
    return NavigationManeuver.none;
  }

  static double? _distance(String text) {
    final match = RegExp(_distancePattern, caseSensitive: false).firstMatch(text);
    if (match == null) return null;
    final value = double.tryParse(match.group(1)!.replaceAll(',', '.'));
    if (value == null) return null;
    return switch (match.group(2)!.toLowerCase()) {
      'km' => value * 1000,
      'mi' => value * 1609.344,
      'ft' => value * 0.3048,
      _ => value,
    };
  }

  static String _roadName(String title, String text, String bigText) {
    for (final candidate in [title, text, bigText]) {
      if (candidate.isEmpty) continue;
      final match = RegExp(
        r'\b(?:onto|toward|towards)\s+(.+)',
        caseSensitive: false,
      ).firstMatch(candidate);
      if (match == null) continue;
      var name = match.group(1)!.trim();
      name = name.replaceAll(RegExp(r'\s+[·|]\s+.*$'), '').trim();
      name = name.replaceAll(RegExp(r'\s+\d.*$'), '').trim();
      if (name.isNotEmpty) return name;
    }
    if (title.isNotEmpty && !RegExp(r'^\d').hasMatch(title)) {
      return title;
    }
    return '';
  }

  static DateTime? _eta(String text, DateTime postedAt) {
    final clock = RegExp(_etaPattern, caseSensitive: false).firstMatch(text);
    if (clock != null) {
      final hour = int.tryParse(clock.group(1)!);
      final minute = int.tryParse(clock.group(2)!);
      if (hour != null && minute != null && hour < 24 && minute < 60) {
        var eta = DateTime(
          postedAt.year,
          postedAt.month,
          postedAt.day,
          hour,
          minute,
        );
        if (eta.isBefore(postedAt)) {
          eta = eta.add(const Duration(days: 1));
        }
        return eta;
      }
    }
    final minutes = RegExp(_minutesPattern).firstMatch(text);
    if (minutes != null) {
      final value = int.tryParse(minutes.group(1)!);
      if (value != null) return postedAt.add(Duration(minutes: value));
    }
    return null;
  }
}
