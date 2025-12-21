import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:juce_mix_player/unified_av_player.dart';

class UnifiedVideoView extends StatefulWidget {
  final UnifiedAVPlayerController controller;
  final VoidCallback? onViewReady;

  const UnifiedVideoView({
    Key? key,
    required this.controller,
    this.onViewReady,
  }) : super(key: key);

  @override
  State<UnifiedVideoView> createState() => _UnifiedVideoViewState();
}

class _UnifiedVideoViewState extends State<UnifiedVideoView> {
  @override
  Widget build(BuildContext context) {
    const String viewType = 'unified-video-view';

    final Map<String, dynamic> creationParams = {
      'playerPtr': widget.controller.getPtr().address,
    };

    if (defaultTargetPlatform == TargetPlatform.iOS) {
      return UiKitView(
        viewType: viewType,
        creationParams: creationParams,
        creationParamsCodec: const StandardMessageCodec(),
        onPlatformViewCreated: _onPlatformViewCreated,
      );
    } else if (defaultTargetPlatform == TargetPlatform.android) {
      return AndroidView(
        viewType: viewType,
        creationParams: creationParams,
        creationParamsCodec: const StandardMessageCodec(),
        onPlatformViewCreated: _onPlatformViewCreated,
      );
    }

    return const Center(
      child: Text('Video playback not supported on this platform'),
    );
  }

  void _onPlatformViewCreated(int viewId) {
    widget.onViewReady?.call();
  }
}
