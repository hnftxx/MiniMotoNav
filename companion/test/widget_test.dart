import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:moto_companion/src/widgets/arc_gauge.dart';

void main() {
  testWidgets('arc gauge renders its value and label', (tester) async {
    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: Center(
            child: ArcGauge(value: 7250, max: 14000, label: 'RPM', unit: ''),
          ),
        ),
      ),
    );

    expect(find.text('7250'), findsOneWidget);
    expect(find.textContaining('RPM'), findsOneWidget);
  });
}
