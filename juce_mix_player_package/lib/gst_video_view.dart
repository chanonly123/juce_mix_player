import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:juce_mix_player/gst_video_player.dart';

class GstVideoView extends StatelessWidget {
  final GstPlayerController controller;
  final VoidCallback? onViewReady;

  const GstVideoView({
    super.key,
    required this.controller,
    this.onViewReady,
  });

  @override
  Widget build(BuildContext context) {
    return UiKitView(
      viewType: "gst-video-view",
      creationParams: {
        'playerPtr': controller.nativeHandle,
      },
      creationParamsCodec: const StandardMessageCodec(),
      onPlatformViewCreated: (int id) {
        // Give the native view a moment to fully initialize
        Future.delayed(const Duration(milliseconds: 100), () {
          onViewReady?.call();
        });
      },
    );
  }
}
