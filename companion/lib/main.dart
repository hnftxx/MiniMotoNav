import 'dart:async';

import 'package:flutter/material.dart';

import 'src/app/splash_screen.dart';
import 'src/app/theme.dart';
import 'src/navigation/maps_notification_service.dart';
import 'src/protocol_service.dart';
import 'src/services/app_settings.dart';
import 'src/shell/app_shell.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await AppSettings.instance.load();
  ProtocolService.instance.start();
  unawaited(MapsNotificationService.instance.start());
  runApp(const MotoCompanionApp());
}

class MotoCompanionApp extends StatelessWidget {
  const MotoCompanionApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'GODSPEED',
      debugShowCheckedModeBanner: false,
      theme: buildMotoTheme(),
      home: const SplashScreen(child: AppShell()),
    );
  }
}
