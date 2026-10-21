import 'dart:math' as math;

import 'package:flutter/material.dart';

class ArcGauge extends StatelessWidget {
  const ArcGauge({
    super.key,
    required this.value,
    required this.max,
    required this.label,
    required this.unit,
    this.redline,
    this.size = 220,
    this.color = const Color(0xFF3AD8FF),
    this.showValue = true,
  });

  final double value;
  final double max;
  final String label;
  final String unit;
  final double? redline;
  final double size;
  final Color color;
  final bool showValue;

  @override
  Widget build(BuildContext context) {
    final clamped = value.clamp(0, max).toDouble();
    return SizedBox(
      width: size,
      height: size * 0.82,
      child: CustomPaint(
        painter: _GaugePainter(
          fraction: max == 0 ? 0 : clamped / max,
          redline: redline == null || max == 0
              ? null
              : (redline! / max).clamp(0, 1).toDouble(),
          color: color,
        ),
        child: Align(
          alignment: const Alignment(0, 0.28),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (showValue)
                Text(
                  value.isFinite ? value.toStringAsFixed(0) : '--',
                  style: TextStyle(
                    fontSize: size * 0.21,
                    fontWeight: FontWeight.w700,
                    height: 1,
                    color: Colors.white,
                    fontFeatures: const [FontFeature.tabularFigures()],
                  ),
                ),
              const SizedBox(height: 2),
              Text(
                '$unit   $label'.trim(),
                style: TextStyle(
                  fontSize: 11,
                  letterSpacing: 1.6,
                  color: Colors.white.withValues(alpha: 0.55),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _GaugePainter extends CustomPainter {
  _GaugePainter({
    required this.fraction,
    required this.redline,
    required this.color,
  });

  final double fraction;
  final double? redline;
  final Color color;

  static const start = math.pi * 0.75;
  static const sweep = math.pi * 1.5;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height * 0.72);
    final radius = math.min(size.width / 2 - 10, size.height * 0.7);
    final rect = Rect.fromCircle(center: center, radius: radius);

    final track = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10
      ..strokeCap = StrokeCap.round
      ..color = Colors.white.withValues(alpha: 0.08);
    canvas.drawArc(rect, start, sweep, false, track);

    if (redline != null && redline! < 1) {
      canvas.drawArc(
        rect,
        start + sweep * redline!,
        sweep * (1 - redline!),
        false,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 10
          ..color = const Color(0xFFE53935).withValues(alpha: 0.55),
      );
    }

    final glow = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 18
      ..strokeCap = StrokeCap.round
      ..color = color.withValues(alpha: 0.14)
      ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 12);
    canvas.drawArc(rect, start, sweep * fraction, false, glow);

    final valuePaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10
      ..strokeCap = StrokeCap.round
      ..shader = SweepGradient(
        startAngle: start,
        endAngle: start + sweep,
        colors: [color.withValues(alpha: 0.35), color],
        transform: const GradientRotation(0),
      ).createShader(rect);
    canvas.drawArc(rect, start, sweep * fraction, false, valuePaint);

    final tickPaint = Paint()
      ..strokeWidth = 1.5
      ..color = Colors.white.withValues(alpha: 0.22);
    for (var i = 0; i <= 10; i++) {
      final angle = start + sweep * i / 10;
      final dir = Offset(math.cos(angle), math.sin(angle));
      final inner = center + dir * (radius - 16);
      final outer = center + dir * (radius - (i % 5 == 0 ? 26 : 22));
      canvas.drawLine(inner, outer, tickPaint);
    }
  }

  @override
  bool shouldRepaint(_GaugePainter old) =>
      old.fraction != fraction || old.redline != redline || old.color != color;
}
