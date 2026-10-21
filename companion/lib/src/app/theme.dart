import 'package:flutter/material.dart';

abstract final class MotoColors {
  static const background = Color(0xFF06090D);
  static const surface = Color(0xFF0D1218);
  static const panel = Color(0xFF111821);
  static const panelHigh = Color(0xFF16202A);
  static const stroke = Color(0xFF1F2C38);
  static const cyan = Color(0xFF3AD8FF);
  static const cyanDim = Color(0xFF1B93B4);
  static const blue = Color(0xFF2F7BFF);
  static const lime = Color(0xFFB8FF4D);
  static const amber = Color(0xFFFFB020);
  static const red = Color(0xFFFF4D57);
  static const text = Color(0xFFE7F2F8);
  static const muted = Color(0xFF7E909F);
  static const faint = Color(0xFF4C5D6B);
}

ThemeData buildMotoTheme() {
  final base = ThemeData(
    brightness: Brightness.dark,
    useMaterial3: true,
    colorScheme: const ColorScheme.dark(
      primary: MotoColors.cyan,
      onPrimary: Color(0xFF00232C),
      secondary: MotoColors.blue,
      onSecondary: Colors.white,
      surface: MotoColors.surface,
      onSurface: MotoColors.text,
      error: MotoColors.red,
      outline: MotoColors.stroke,
    ),
    scaffoldBackgroundColor: MotoColors.background,
    fontFamily: 'Roboto',
  );

  return base.copyWith(
    textTheme: base.textTheme.apply(
      bodyColor: MotoColors.text,
      displayColor: MotoColors.text,
    ),
    appBarTheme: const AppBarTheme(
      backgroundColor: MotoColors.background,
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      centerTitle: false,
    ),
    drawerTheme: const DrawerThemeData(
      backgroundColor: MotoColors.surface,
      width: 306,
    ),
    dividerTheme: const DividerThemeData(
      color: MotoColors.stroke,
      space: 1,
      thickness: 1,
    ),
    cardTheme: CardThemeData(
      color: MotoColors.panel,
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: MotoColors.stroke),
      ),
    ),
    sliderTheme: base.sliderTheme.copyWith(
      trackHeight: 3,
      activeTrackColor: MotoColors.cyan,
      inactiveTrackColor: MotoColors.stroke,
      thumbColor: MotoColors.cyan,
      overlayColor: MotoColors.cyan.withValues(alpha: 0.12),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: MotoColors.panel,
      contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: MotoColors.stroke),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: MotoColors.stroke),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: MotoColors.cyan),
      ),
      labelStyle: const TextStyle(
        color: MotoColors.muted,
        fontSize: 12,
        letterSpacing: 0.6,
      ),
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: MotoColors.cyan,
        foregroundColor: const Color(0xFF00232C),
        minimumSize: const Size(0, 46),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        textStyle: const TextStyle(
          fontWeight: FontWeight.w700,
          letterSpacing: 0.8,
          fontSize: 12,
        ),
      ),
    ),
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: MotoColors.text,
        minimumSize: const Size(0, 46),
        side: const BorderSide(color: MotoColors.stroke),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        textStyle: const TextStyle(
          fontWeight: FontWeight.w600,
          letterSpacing: 0.8,
          fontSize: 12,
        ),
      ),
    ),
    bottomSheetTheme: const BottomSheetThemeData(
      backgroundColor: MotoColors.panel,
      surfaceTintColor: Colors.transparent,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
    ),
    snackBarTheme: const SnackBarThemeData(
      backgroundColor: MotoColors.panelHigh,
      contentTextStyle: TextStyle(color: MotoColors.text),
      behavior: SnackBarBehavior.floating,
    ),
    searchBarTheme: SearchBarThemeData(
      backgroundColor: WidgetStatePropertyAll(MotoColors.panel),
      elevation: const WidgetStatePropertyAll(0),
      side: const WidgetStatePropertyAll(BorderSide(color: MotoColors.stroke)),
      shape: WidgetStatePropertyAll(
        RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      ),
    ),
  );
}

TextStyle monoStyle({
  double size = 13,
  FontWeight weight = FontWeight.w600,
  Color color = MotoColors.text,
  double letterSpacing = 0,
}) => TextStyle(
  fontSize: size,
  fontWeight: weight,
  color: color,
  letterSpacing: letterSpacing,
  fontFeatures: const [FontFeature.tabularFigures()],
);

TextStyle labelStyle({
  Color color = MotoColors.muted,
  double size = 10,
  double spacing = 1.6,
}) => TextStyle(
  fontSize: size,
  letterSpacing: spacing,
  fontWeight: FontWeight.w600,
  color: color,
);
