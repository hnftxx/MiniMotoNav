import 'dart:async';

import 'package:flutter/material.dart';
import 'package:video_player/video_player.dart';

import 'theme.dart';

class SplashScreen extends StatefulWidget {
  const SplashScreen({super.key, required this.child});

  final Widget child;

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen> {
  static const _asset = 'assets/video/godspeed_splash.mp4';
  static const _maxWait = Duration(seconds: 4);

  VideoPlayerController? _video;
  Timer? _deadline;
  bool _ready = false;
  bool _leaving = false;

  @override
  void initState() {
    super.initState();
    _deadline = Timer(_maxWait, _finish);
    _start();
  }

  Future<void> _start() async {
    final controller = VideoPlayerController.asset(_asset);
    try {
      await controller.initialize();
      await controller.setVolume(0);
      if (!mounted) {
        await controller.dispose();
        return;
      }
      _video = controller;
      controller.addListener(_onVideoTick);
      await controller.play();
      setState(() => _ready = true);
    } catch (_) {
      await controller.dispose();
    }
  }

  void _onVideoTick() {
    final video = _video;
    if (video == null || _leaving) return;
    final value = video.value;
    if (value.isCompleted || (value.duration > Duration.zero &&
        value.position >= value.duration)) {
      _finish();
    }
  }

  void _finish() {
    if (_leaving || !mounted) return;
    _leaving = true;
    _deadline?.cancel();
    Navigator.of(context).pushReplacement(
      PageRouteBuilder<void>(
        transitionDuration: const Duration(milliseconds: 420),
        pageBuilder: (_, _, _) => widget.child,
        transitionsBuilder: (_, animation, _, child) =>
            FadeTransition(opacity: animation, child: child),
      ),
    );
  }

  @override
  void dispose() {
    _deadline?.cancel();
    _video?.removeListener(_onVideoTick);
    _video?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final video = _video;
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: _finish,
      child: ColoredBox(
        color: MotoColors.background,
        child: Stack(
          fit: StackFit.expand,
          children: [
            Center(
              child: AnimatedOpacity(
                opacity: _ready ? 1 : 0,
                duration: const Duration(milliseconds: 260),
                child: video == null
                    ? const SizedBox.shrink()
                    : AspectRatio(
                        aspectRatio: video.value.aspectRatio,
                        child: VideoPlayer(video),
                      ),
              ),
            ),
            if (!_ready)
              Center(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 48),
                  child: Image.asset(
                    'assets/brand/godspeed_logo.png',
                    fit: BoxFit.contain,
                  ),
                ),
              ),
            Align(
              alignment: Alignment.bottomCenter,
              child: Padding(
                padding: const EdgeInsets.only(bottom: 28),
                child: Text(
                  'TAP TO CONTINUE',
                  style: TextStyle(
                    color: MotoColors.muted.withValues(alpha: 0.7),
                    fontSize: 10,
                    letterSpacing: 3,
                    fontWeight: FontWeight.w600,
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
