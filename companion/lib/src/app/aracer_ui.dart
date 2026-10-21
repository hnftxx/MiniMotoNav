import 'package:flutter/material.dart';

/// Visual language for the ECU settings screens.
///
/// Rebuilt from the layout and colour vocabulary of the aRacer X Tune 3.7.2
/// settings surface (carbon backdrop, yellow title band, circular blue icon
/// chips, 50 dp list cards). No aRacer assets, fonts or branding are copied:
/// the carbon weave is painted procedurally and every label is our own.
abstract final class ARacerColors {
  static const carbon = Color(0xFF121212);
  static const carbonWeave = Color(0xFF1C1C1C);
  static const carbonHighlight = Color(0xFF262626);
  static const headerYellow = Color(0xFFFEEA39);
  static const yellow600 = Color(0xFFF4BD1A);
  static const yellow700 = Color(0xFFF4A81C);
  static const yellow800 = Color(0xFFF19320);
  static const iconBlue = Color(0xFF0091EA);
  static const blue600 = Color(0xFF3D82C4);
  static const blue700 = Color(0xFF3374BA);
  static const cyan400 = Color(0xFF35C1D6);
  static const green600 = Color(0xFF42A046);
  static const limeA200 = Color(0xFFE8E63E);
  static const grey400 = Color(0xFFBDBCBC);
  static const lightGreenA400 = Color(0xFF8EC63E);
  static const red800 = Color(0xFFC52828);
  static const text = Color(0xFFFFFFFF);
  static const detail = Color(0xFFCDC5C4);
  static const transparent = Color(0x00000000);
}

/// Painted carbon-fibre twill. Used instead of a bitmap so the surface stays
/// resolution independent and ships no third-party artwork.
class CarbonBackground extends StatelessWidget {
  const CarbonBackground({super.key, this.child});

  final Widget? child;

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      painter: const _CarbonPainter(),
      isComplex: true,
      willChange: false,
      child: child,
    );
  }
}

class _CarbonPainter extends CustomPainter {
  const _CarbonPainter();

  static const double _cell = 12;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(
      Offset.zero & size,
      Paint()..color = ARacerColors.carbon,
    );

    // Twill weave: alternating diagonal bundles give the carbon look.
    for (var row = 0; row * _cell < size.height + _cell; row++) {
      final y = row * _cell;
      final bundle = Paint()
        ..strokeWidth = _cell / 2
        ..color = row.isEven
            ? ARacerColors.carbonWeave
            : ARacerColors.carbonHighlight;
      for (var x = -_cell; x < size.width + _cell; x += _cell * 2) {
        final offset = row.isEven ? 0.0 : _cell;
        canvas.drawLine(
          Offset(x + offset, y),
          Offset(x + offset + _cell, y + _cell / 2),
          bundle,
        );
      }
    }
    final light = Paint()
      ..color = ARacerColors.carbonWeave
      ..strokeWidth = 1;
    final dark = Paint()
      ..color = const Color(0xFF0B0B0B)
      ..strokeWidth = 1;
    for (var x = 0.0; x < size.width + _cell; x += _cell * 3) {
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), light);
      canvas.drawLine(Offset(x + 1.5, 0), Offset(x + 1.5, size.height), dark);
    }
  }

  @override
  bool shouldRepaint(_CarbonPainter old) => false;
}

/// The 50 dp yellow title band used by every aRacer page.
class ARacerHeader extends StatelessWidget {
  const ARacerHeader({
    super.key,
    required this.title,
    this.statusLabel,
    this.statusOn = false,
    this.onStatusTap,
  });

  final String title;
  final String? statusLabel;
  final bool statusOn;
  final VoidCallback? onStatusTap;

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 50,
      color: ARacerColors.headerYellow,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      child: Row(
        children: [
          Container(width: 4, height: 26, color: const Color(0xFF1A1A1A)),
          const SizedBox(width: 10),
          Text(
            title,
            style: const TextStyle(
              fontSize: 19,
              fontWeight: FontWeight.w800,
              letterSpacing: 1.4,
              color: Color(0xFF141414),
            ),
          ),
          const Spacer(),
          if (statusLabel != null)
            InkWell(
              onTap: onStatusTap,
              child: Row(
                children: [
                  Icon(
                    Icons.wifi,
                    size: 18,
                    color: statusOn
                        ? const Color(0xFF1A1A1A)
                        : const Color(0xFF7C7C7C),
                  ),
                  const SizedBox(width: 6),
                  Text(
                    statusLabel!,
                    style: TextStyle(
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 1,
                      color: statusOn
                          ? const Color(0xFF1A1A1A)
                          : const Color(0xFF6E6E6E),
                    ),
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }
}

/// Thin yellow rule with the ECU serial strip, mirroring the target-ECU band.
class ARacerSerialStrip extends StatelessWidget {
  const ARacerSerialStrip({
    super.key,
    required this.serial,
    this.onCopy,
  });

  final String serial;
  final VoidCallback? onCopy;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(16, 8, 8, 8),
      decoration: const BoxDecoration(
        border: Border(
          bottom: BorderSide(color: ARacerColors.yellow700, width: 2),
        ),
      ),
      child: Row(
        children: [
          Expanded(
            child: Text(
              'ECU Serial: $serial',
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.w600,
                color: ARacerColors.yellow700,
              ),
            ),
          ),
          TextButton(
            onPressed: onCopy,
            style: TextButton.styleFrom(
              foregroundColor: ARacerColors.red800,
              minimumSize: const Size(0, 32),
              padding: const EdgeInsets.symmetric(horizontal: 10),
            ),
            child: const Text(
              'Copy',
              style: TextStyle(fontSize: 12, fontWeight: FontWeight.w700),
            ),
          ),
        ],
      ),
    );
  }
}

/// One settings row: circular icon chip, title, optional detail, optional
/// trailing widget. Matches the aRacer 8 dp card with 50 dp content height.
class ARacerCard extends StatelessWidget {
  const ARacerCard({
    super.key,
    required this.icon,
    required this.title,
    this.detail = '',
    this.onTap,
    this.trailing,
    this.titleColor = ARacerColors.text,
    this.iconColor = ARacerColors.iconBlue,
    this.enabled = true,
    this.showChevron = false,
  });

  final IconData icon;
  final String title;
  final String detail;
  final VoidCallback? onTap;
  final Widget? trailing;
  final Color titleColor;
  final Color iconColor;
  final bool enabled;
  final bool showChevron;

  @override
  Widget build(BuildContext context) {
    final effectiveTitle = enabled ? titleColor : ARacerColors.grey400;
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
      child: Material(
        color: ARacerColors.transparent,
        borderRadius: BorderRadius.circular(8),
        child: InkWell(
          borderRadius: BorderRadius.circular(8),
          onTap: enabled ? onTap : null,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 7),
            child: Row(
              children: [
                Container(
                  width: 36,
                  height: 36,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: enabled ? iconColor : ARacerColors.grey400,
                  ),
                  child: Icon(icon, size: 20, color: Colors.white),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        title,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: TextStyle(
                          fontSize: 15,
                          fontWeight: FontWeight.w600,
                          color: effectiveTitle,
                        ),
                      ),
                      if (detail.isNotEmpty) ...[
                        const SizedBox(height: 1),
                        Text(
                          detail,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(
                            fontSize: 11.5,
                            color: ARacerColors.detail,
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
                ?trailing,
                if (showChevron)
                  const Padding(
                    padding: EdgeInsets.only(left: 4),
                    child: Icon(
                      Icons.chevron_right,
                      size: 20,
                      color: ARacerColors.grey400,
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

/// Blue Material switch used by the aRacer setting rows.
class ARacerSwitch extends StatelessWidget {
  const ARacerSwitch({
    super.key,
    required this.value,
    required this.onChanged,
  });

  final bool value;
  final ValueChanged<bool>? onChanged;

  @override
  Widget build(BuildContext context) {
    return Switch(
      value: value,
      onChanged: onChanged,
      activeThumbColor: ARacerColors.blue600,
      activeTrackColor: ARacerColors.blue700.withValues(alpha: 0.45),
      inactiveThumbColor: ARacerColors.grey400,
      inactiveTrackColor: const Color(0xFF3A3A3A),
      materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
    );
  }
}

/// "Maximum RPM scale" row: three tappable 10000 / 12000 / 16000 cells.
class ARacerScaleRow extends StatelessWidget {
  const ARacerScaleRow({
    super.key,
    required this.title,
    required this.value,
    required this.options,
    required this.onChanged,
  });

  final String title;
  final int value;
  final List<int> options;
  final ValueChanged<int> onChanged;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
      child: Container(
        decoration: BoxDecoration(
          color: const Color(0x1AFFFFFF),
          borderRadius: BorderRadius.circular(8),
        ),
        padding: const EdgeInsets.fromLTRB(8, 8, 8, 8),
        child: Row(
          children: [
            Container(
              width: 36,
              height: 36,
              decoration: const BoxDecoration(
                shape: BoxShape.circle,
                color: ARacerColors.iconBlue,
              ),
              child: const Icon(
                Icons.speed,
                size: 20,
                color: Colors.white,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              flex: 3,
              child: Text(
                title,
                maxLines: 2,
                style: const TextStyle(
                  fontSize: 14.5,
                  fontWeight: FontWeight.w600,
                  color: ARacerColors.text,
                ),
              ),
            ),
            for (final option in options)
              Expanded(
                flex: 2,
                child: Padding(
                  padding: const EdgeInsets.only(left: 4),
                  child: InkWell(
                    borderRadius: BorderRadius.circular(6),
                    onTap: () => onChanged(option),
                    child: Container(
                      height: 34,
                      alignment: Alignment.center,
                      decoration: BoxDecoration(
                        color: option == value
                            ? ARacerColors.limeA200.withValues(alpha: 0.55)
                            : const Color(0x56ACaeae),
                        borderRadius: BorderRadius.circular(6),
                      ),
                      child: Text(
                        '$option',
                        style: TextStyle(
                          fontSize: 12.5,
                          fontWeight: FontWeight.w700,
                          color: option == value
                              ? const Color(0xFF1A1A1A)
                              : Colors.white,
                        ),
                      ),
                    ),
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }
}

/// Section caption used between card groups.
class ARacerSectionLabel extends StatelessWidget {
  const ARacerSectionLabel(this.text, {super.key, this.trailing});

  final String text;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(14, 14, 14, 6),
      child: Row(
        children: [
          Text(
            text.toUpperCase(),
            style: const TextStyle(
              fontSize: 10,
              letterSpacing: 1.8,
              fontWeight: FontWeight.w700,
              color: ARacerColors.yellow700,
            ),
          ),
          const Spacer(),
          ?trailing,
        ],
      ),
    );
  }
}

/// Carbon-panel dialog with the aRacer yellow title, used for unit, language
/// and destructive-action confirmations.
Future<T?> showARacerDialog<T>({
  required BuildContext context,
  required String title,
  required Widget child,
  List<Widget> actions = const [],
}) {
  return showDialog<T>(
    context: context,
    builder: (context) => Dialog(
      backgroundColor: const Color(0xFF1B1B1B),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Container(
            color: ARacerColors.headerYellow,
            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
            child: Text(
              title,
              style: const TextStyle(
                fontSize: 15,
                fontWeight: FontWeight.w800,
                color: Color(0xFF141414),
              ),
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(14, 14, 14, 8),
            child: child,
          ),
          if (actions.isNotEmpty)
            Padding(
              padding: const EdgeInsets.fromLTRB(10, 0, 10, 10),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: actions,
              ),
            ),
        ],
      ),
    ),
  );
}

/// Yellow action button for dialogs.
class ARacerDialogAction extends StatelessWidget {
  const ARacerDialogAction({
    super.key,
    required this.label,
    this.onPressed,
    this.color = ARacerColors.headerYellow,
  });

  final String label;
  final VoidCallback? onPressed;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return TextButton(
      onPressed: onPressed,
      style: TextButton.styleFrom(
        foregroundColor: const Color(0xFF141414),
        backgroundColor: onPressed == null ? const Color(0xFF4A4A4A) : color,
        minimumSize: const Size(0, 38),
        padding: const EdgeInsets.symmetric(horizontal: 18),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(6)),
      ),
      child: Text(
        label,
        style: const TextStyle(fontSize: 12.5, fontWeight: FontWeight.w800),
      ),
    );
  }
}
