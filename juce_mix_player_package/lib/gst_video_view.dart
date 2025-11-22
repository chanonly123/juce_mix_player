import 'dart:io' show Platform;
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
    const String viewType = "gst-video-view";
    final Map<String, dynamic> creationParams = {
      'playerPtr': controller.nativeHandle,
    };

    // Platform-specific view creation
    if (Platform.isIOS) {
      return UiKitView(
        viewType: viewType,
        creationParams: creationParams,
        creationParamsCodec: const StandardMessageCodec(),
        onPlatformViewCreated: _onPlatformViewCreated,
      );
    } else if (Platform.isAndroid) {
      return AndroidView(
        viewType: viewType,
        creationParams: creationParams,
        creationParamsCodec: const StandardMessageCodec(),
        onPlatformViewCreated: _onPlatformViewCreated,
      );
    } else {
      // Fallback for unsupported platforms
      return Container(
        color: Colors.black,
        child: const Center(
          child: Text(
            'Platform not supported',
            style: TextStyle(color: Colors.white),
          ),
        ),
      );
    }
  }

  void _onPlatformViewCreated(int id) {
    // Give the native view a moment to fully initialize
    Future.delayed(const Duration(milliseconds: 100), () {
      onViewReady?.call();
    });
  }
}
