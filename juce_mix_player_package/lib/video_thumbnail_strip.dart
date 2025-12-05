// -----------------------------------------------------------
// Video Thumbnail Strip
// -----------------------------------------------------------

import 'dart:async';
import 'dart:collection';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:video_thumbnail/video_thumbnail.dart';
import 'package:path_provider/path_provider.dart';
import 'package:flutter/services.dart';

/// Trim data returned in callbacks
class TrimData {
  final int startMs;
  final int endMs;
  final int durationMs;
  final int totalVideoDurationMs;

  TrimData({
    required this.startMs,
    required this.endMs,
    required this.durationMs,
    required this.totalVideoDurationMs,
  });

  Duration get start => Duration(milliseconds: startMs);
  Duration get end => Duration(milliseconds: endMs);
  Duration get duration => Duration(milliseconds: durationMs);
  Duration get totalDuration => Duration(milliseconds: totalVideoDurationMs);

  @override
  String toString() =>
      'TrimData(start: ${startMs}ms, end: ${endMs}ms, duration: ${durationMs}ms, total: ${totalVideoDurationMs}ms)';
}

/// Controller to interact with the trimmer programmatically
class VideoThumbnailStripController {
  void Function(int startMs, int endMs)? _internalSetTrimRange;
  void Function()? _internalDispose;

  void setTrimRange({required int startMs, required int endMs}) {
    _internalSetTrimRange?.call(startMs, endMs);
  }

  void _setInternalHooks({
    void Function(int, int)? setTrimRange,
    void Function()? dispose,
  }) {
    _internalSetTrimRange = setTrimRange;
    _internalDispose = dispose;
  }

  void dispose() {
    _internalDispose?.call();
    _internalSetTrimRange = null;
    _internalDispose = null;
  }
}

class VideoThumbnailStrip extends StatefulWidget {
  final String filePath;
  final double width;
  final double height;
  final int thumbnailCount;
  final double spacing;
  final BorderRadius? borderRadius;
  final Duration videoDuration;
  final VideoThumbnailStripController? controller;
  final Color? backgroundColor;
  final BoxFit fit;
  final int maxMemoryCache;

  // Trim specific properties
  final void Function(TrimData trimData)? onTrimChangeEnd;
  final int? maxTrimDurationMs;
  final int initialStartMs;
  final int initialEndMs;
  final Gradient? windowGradient;
  final double handleWidth;

  VideoThumbnailStrip({
    Key? key,
    required this.filePath,
    required this.videoDuration,
    this.width = double.infinity,
    this.height = 50,
    this.thumbnailCount = 15,
    this.spacing = 0,
    this.borderRadius,
    this.controller,
    this.backgroundColor,
    this.fit = BoxFit.cover,
    this.maxMemoryCache = 100,
    this.onTrimChangeEnd,
    this.maxTrimDurationMs,
    this.initialStartMs = 0,
    int? initialEndMs,
    this.windowGradient,
    this.handleWidth = 4.0,
  })  : initialEndMs = initialEndMs ?? videoDuration.inMilliseconds,
        assert(thumbnailCount > 0),
        assert(maxMemoryCache > 0),
        assert(initialStartMs >= 0),
        super(key: key);

  @override
  State<VideoThumbnailStrip> createState() => _VideoThumbnailStripState();
}

class _VideoThumbnailStripState extends State<VideoThumbnailStrip> {
  Duration? _duration;
  bool _loading = true;
  bool _disposed = false;

  final LinkedHashMap<String, Uint8List> _memoryCache = LinkedHashMap();
  Directory? _cacheDir;
  bool _preloading = false;

  late int _trimStartMs;
  late int _trimEndMs;

  // Time labels (chips) above the strip
  bool _showTimeLabels = false;
  Timer? _labelTimer;

  @override
  void initState() {
    super.initState();
    _duration = widget.videoDuration;
    _trimStartMs = widget.initialStartMs;
    _trimEndMs = widget.initialEndMs;

    widget.controller?._setInternalHooks(
      setTrimRange: _programmaticSetTrimRange,
      dispose: _controllerDispose,
    );

    _init();
  }

  Future<void> _init() async {
    _cacheDir = await _makeCacheDir();
    _startSequentialPreload();
    if (!_disposed) setState(() => _loading = false);
  }

  Future<Directory> _makeCacheDir() async {
    final tmp = await getTemporaryDirectory();
    final videoHash = widget.filePath.hashCode.toUnsigned(32).toString();
    final dir = Directory('${tmp.path}/video_thumb_cache_flutter_$videoHash');
    if (!await dir.exists()) await dir.create(recursive: true);
    return dir;
  }

  String _diskThumbPathForIndex(int index) => '${_cacheDir!.path}/thumb_$index.png';

  Future<void> _startSequentialPreload() async {
    if (_preloading || _duration == null) return;
    _preloading = true;
    final totalMs = _duration!.inMilliseconds;

    for (var i = 0; i < widget.thumbnailCount; i++) {
      if (_disposed) break;

      final key = 'i_$i';
      if (_memoryCache.containsKey(key)) continue;

      final timeMs = (totalMs * (i / (widget.thumbnailCount - 1))).round();
      final diskPath = _diskThumbPathForIndex(i);

      final f = File(diskPath);
      if (await f.exists()) {
        try {
          final bytes = await f.readAsBytes();
          _addToMemoryCache(key, bytes);
          if (!_disposed) setState(() {});
          continue;
        } catch (_) {}
      }

      await _generateFrameToDisk(timeMs, diskPath);
      if (_disposed) break;

      if (await File(diskPath).exists()) {
        try {
          final bytes = await File(diskPath).readAsBytes();
          _addToMemoryCache(key, bytes);
          if (!_disposed) setState(() {});
        } catch (_) {}
      }

      await Future.delayed(const Duration(milliseconds: 50));
    }

    _preloading = false;
  }

  Future<void> _generateFrameToDisk(int timeMs, String outPath) async {
    try {
      final data = await VideoThumbnail.thumbnailData(
        video: widget.filePath,
        imageFormat: ImageFormat.WEBP,
        timeMs: timeMs,
        quality: 80,
        maxHeight: (widget.height * 2).toInt(),
      );

      if (data != null) {
        await File(outPath).writeAsBytes(data);
      }
    } catch (_) {}
  }

  void _addToMemoryCache(String key, Uint8List bytes) {
    if (_memoryCache.containsKey(key)) {
      final existing = _memoryCache.remove(key);
      if (existing != null) _memoryCache[key] = existing;
      return;
    }
    _memoryCache[key] = bytes;

    while (_memoryCache.length > widget.maxMemoryCache) {
      _memoryCache.remove(_memoryCache.keys.first);
    }
  }

  void _controllerDispose() {
    _disposed = true;
    _memoryCache.clear();
  }

  void _programmaticSetTrimRange(int startMs, int endMs) {
    if (_duration == null) return;
    final totalMs = _duration!.inMilliseconds;

    setState(() {
      _trimStartMs = startMs.clamp(0, totalMs);
      _trimEndMs = endMs.clamp(0, totalMs);

      if (_trimStartMs >= _trimEndMs) {
        _trimEndMs = (_trimStartMs + 1000).clamp(0, totalMs);
      }
    });
  }

  @override
  void dispose() {
    _disposed = true;
    _labelTimer?.cancel();
    widget.controller?._setInternalHooks(setTrimRange: null, dispose: null);
    super.dispose();
  }

  void _resetLabelTimer() {
    _labelTimer?.cancel();
    setState(() => _showTimeLabels = true);
    _labelTimer = Timer(const Duration(seconds: 3), () {
      if (mounted) setState(() => _showTimeLabels = false);
    });
  }

  void _notifyTrimChanged() {
    final trimData = TrimData(
      startMs: _trimStartMs,
      endMs: _trimEndMs,
      durationMs: _trimEndMs - _trimStartMs,
      totalVideoDurationMs: _duration!.inMilliseconds,
    );
    widget.onTrimChangeEnd?.call(trimData);
  }

  String _formatDuration(Duration d) {
    final minutes = d.inMinutes.remainder(60).toString().padLeft(2, '0');
    final seconds = d.inSeconds.remainder(60).toString().padLeft(2, '0');
    final milliseconds = d.inMilliseconds.remainder(1000).toString().padLeft(3, '0');
    return '$minutes:$seconds.$milliseconds';
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final containerWidth = widget.width.isFinite ? widget.width : constraints.maxWidth;

        return Stack(
          clipBehavior: Clip.none, // needed so chips can float above
          children: [
            Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                _buildStrip(containerWidth),
                const SizedBox(height: 6),
                _buildDurationInfo(),
              ],
            ),
            if (_showTimeLabels) _buildFloatingChips(containerWidth),
          ],
        );
      },
    );
  }

  // Floating time chips (outside strip, move with handles)
  Widget _buildFloatingChips(double containerWidth) {
    if (_duration == null) return const SizedBox.shrink();

    final totalMs = _duration!.inMilliseconds;
    final leftFraction = _trimStartMs / totalMs;
    final rightFraction = _trimEndMs / totalMs;

    final leftPos = leftFraction * containerWidth - 50;
    final rightPos = rightFraction * containerWidth - 50;

    return Positioned(
      top: -32,
      left: 0,
      right: 0,
      height: 28,
      child: IgnorePointer(
        ignoring: true,
        child: Stack(
          children: [
            Positioned(
              left: leftPos,
              width: 100,
              child: Center(
                child: _buildTimeLabel(Duration(milliseconds: _trimStartMs)),
              ),
            ),
            Positioned(
              left: rightPos,
              width: 100,
              child: Center(
                child: _buildTimeLabel(Duration(milliseconds: _trimEndMs)),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildTimeLabel(Duration d) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
      decoration: BoxDecoration(
        color: Colors.black.withOpacity(0.75),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Text(
        _formatDuration(d),
        style: const TextStyle(
          color: Colors.white,
          fontSize: 11,
          fontWeight: FontWeight.w500,
        ),
      ),
    );
  }

  Widget _buildStrip(double containerWidth) {
    return SizedBox(
      width: containerWidth,
      height: widget.height,
      child: ClipRRect(
        borderRadius: widget.borderRadius ?? BorderRadius.circular(6),
        child: Container(
          color: widget.backgroundColor ?? Colors.black12,
          child: _loading
              ? const Center(
                  child: SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
                )
              : Stack(
                  fit: StackFit.expand,
                  children: [
                    _buildThumbnailsRow(containerWidth),
                    _buildTrimOverlay(containerWidth),
                  ],
                ),
        ),
      ),
    );
  }

  Widget _buildDurationInfo() {
    if (_duration == null) return const SizedBox.shrink();
    final trimDuration = Duration(milliseconds: _trimEndMs - _trimStartMs);
    return Text(
      '${_formatDuration(trimDuration)} / ${_formatDuration(_duration!)}',
      style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600, color: Colors.white70),
    );
  }

  Widget _buildThumbnailsRow(double containerWidth) {
    final count = widget.thumbnailCount;
    final thumbWidth = (containerWidth - (widget.spacing * (count - 1))) / count;

    final thumbs = List<Widget>.generate(count, (i) {
      final key = 'i_$i';
      final data = _memoryCache.containsKey(key) ? _memoryCache[key] : null;

      final child = data != null
          ? Image.memory(data, fit: widget.fit, width: thumbWidth, height: widget.height)
          : Container(
              width: thumbWidth,
              height: widget.height,
              color: Colors.black26,
              child: const Center(
                child: Icon(Icons.videocam, size: 16, color: Colors.white38),
              ),
            );

      return Padding(
        padding: EdgeInsets.only(right: i == count - 1 ? 0 : widget.spacing),
        child: child,
      );
    });

    return Row(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: thumbs,
    );
  }

  Widget _buildTrimOverlay(double containerWidth) {
    if (_duration == null) return const SizedBox.shrink();

    final totalMs = _duration!.inMilliseconds;
    final leftFraction = _trimStartMs / totalMs;
    final rightFraction = _trimEndMs / totalMs;

    final leftPos = leftFraction * containerWidth;
    final rightPos = rightFraction * containerWidth;
    final trimWidth = rightPos - leftPos;

    const double hitTestWidth = 40.0;

    return Stack(
      clipBehavior: Clip.none,
      children: [
        // Dark overlay on left
        if (leftPos > 0)
          Positioned(
            left: 0,
            top: 0,
            bottom: 0,
            width: leftPos,
            child: Container(color: Colors.black54),
          ),

        // Dark overlay on right
        if (rightPos < containerWidth)
          Positioned(
            left: rightPos,
            top: 0,
            bottom: 0,
            right: 0,
            child: Container(color: Colors.black54),
          ),

        // Trim window box with rounded border and center drag
        Positioned(
          left: leftPos,
          top: 0,
          bottom: 0,
          width: trimWidth,
          child: GestureDetector(
            behavior: HitTestBehavior.opaque,
            onHorizontalDragStart: (_) {
              _resetLabelTimer();
            },
            onHorizontalDragUpdate: (details) {
              _updateTrimWindow(details.delta.dx, containerWidth);
            },
            onHorizontalDragEnd: (_) {
              _notifyTrimChanged();
            },
            child: CustomPaint(
              painter: GradientBorderPainter(
                gradient: widget.windowGradient ?? const LinearGradient(colors: [Colors.white, Colors.white]),
                width: 2,
                radius: 8, // rounded trim box
              ),
            ),
          ),
        ),

        // Left handle
        Positioned(
          left: leftPos - hitTestWidth / 2,
          top: -4,
          bottom: -4,
          width: hitTestWidth,
          child: GestureDetector(
            behavior: HitTestBehavior.opaque,
            onHorizontalDragStart: (_) {
              _resetLabelTimer();
            },
            onHorizontalDragUpdate: (details) {
              _updateLeftHandle(details.delta.dx, containerWidth);
            },
            onHorizontalDragEnd: (_) {
              _notifyTrimChanged();
            },
            child: _buildHandle(true),
          ),
        ),

        // Right handle
        Positioned(
          left: rightPos - hitTestWidth / 2,
          top: -4,
          bottom: -4,
          width: hitTestWidth,
          child: GestureDetector(
            behavior: HitTestBehavior.opaque,
            onHorizontalDragStart: (_) {
              _resetLabelTimer();
            },
            onHorizontalDragUpdate: (details) {
              _updateRightHandle(details.delta.dx, containerWidth);
            },
            onHorizontalDragEnd: (_) {
              _notifyTrimChanged();
            },
            child: _buildHandle(false),
          ),
        ),
      ],
    );
  }

  // Handle with vertical bar + round thumb tip at center
  Widget _buildHandle(bool isLeft) {
    final gradient = widget.windowGradient ?? const LinearGradient(colors: [Colors.white, Colors.white]);

    return Container(
      color: Colors.transparent, // hit test area
      child: Stack(
        alignment: Alignment.center,
        children: [
          // Vertical line
          Container(
            width: widget.handleWidth,
            decoration: BoxDecoration(
              gradient: gradient,
            ),
          ),
          // Round thumb tip at vertical center
          Align(
            alignment: Alignment.center,
            child: Container(
              width: 12,
              height: 12,
              decoration: BoxDecoration(
                gradient: gradient,
                shape: BoxShape.circle,
                boxShadow: const [
                  BoxShadow(
                    color: Colors.black26,
                    blurRadius: 2,
                    offset: Offset(0, 1),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  void _updateLeftHandle(double dx, double containerWidth) {
    if (_duration == null) return;

    final totalMs = _duration!.inMilliseconds;
    final msPerPixel = totalMs / containerWidth;
    final deltaMs = (dx * msPerPixel).round();

    int newStart = _trimStartMs + deltaMs;
    const int minDuration = 1000;
    final int maxStart = _trimEndMs - minDuration;

    bool hitMaxDuration = false;
    if (widget.maxTrimDurationMs != null) {
      final int limitStart = _trimEndMs - widget.maxTrimDurationMs!;
      if (newStart < limitStart) {
        newStart = limitStart;
        hitMaxDuration = true;
      }
    }

    if (newStart > maxStart) {
      newStart = maxStart;
    }

    if (newStart < 0) newStart = 0;

    if (newStart != _trimStartMs) {
      setState(() => _trimStartMs = newStart);
      _resetLabelTimer();
      if (hitMaxDuration) {
        HapticFeedback.heavyImpact();
      } else {
        HapticFeedback.selectionClick();
      }
    }
  }

  void _updateRightHandle(double dx, double containerWidth) {
    if (_duration == null) return;

    final totalMs = _duration!.inMilliseconds;
    final msPerPixel = totalMs / containerWidth;
    final deltaMs = (dx * msPerPixel).round();

    int newEnd = _trimEndMs + deltaMs;
    const int minDuration = 1000;
    final int minEnd = _trimStartMs + minDuration;

    bool hitMaxDuration = false;
    if (widget.maxTrimDurationMs != null) {
      final int limitEnd = _trimStartMs + widget.maxTrimDurationMs!;
      if (newEnd > limitEnd) {
        newEnd = limitEnd;
        hitMaxDuration = true;
      }
    }

    if (newEnd < minEnd) {
      newEnd = minEnd;
    }

    if (newEnd > totalMs) newEnd = totalMs;

    if (newEnd != _trimEndMs) {
      setState(() => _trimEndMs = newEnd);
      _resetLabelTimer();
      if (hitMaxDuration) {
        HapticFeedback.heavyImpact();
      } else {
        HapticFeedback.selectionClick();
      }
    }
  }

  void _updateTrimWindow(double dx, double containerWidth) {
    if (_duration == null) return;

    final totalMs = _duration!.inMilliseconds;
    final msPerPixel = totalMs / containerWidth;
    final deltaMs = (dx * msPerPixel).round();
    final currentDuration = _trimEndMs - _trimStartMs;

    int newStart = _trimStartMs + deltaMs;
    int newEnd = _trimEndMs + deltaMs;

    if (newStart < 0) {
      newStart = 0;
      newEnd = newStart + currentDuration;
    } else if (newEnd > totalMs) {
      newEnd = totalMs;
      newStart = newEnd - currentDuration;
    }

    if (newStart != _trimStartMs) {
      setState(() {
        _trimStartMs = newStart;
        _trimEndMs = newEnd;
      });
      _resetLabelTimer();
      HapticFeedback.selectionClick();
    }
  }
}

class GradientBorderPainter extends CustomPainter {
  final Gradient gradient;
  final double width;
  final double radius;

  GradientBorderPainter({
    required this.gradient,
    this.width = 2,
    this.radius = 8,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final rect = Offset.zero & size;
    final rrect = RRect.fromRectAndRadius(
      rect.deflate(width / 2),
      Radius.circular(radius),
    );

    final paint = Paint()
      ..shader = gradient.createShader(rect)
      ..style = PaintingStyle.stroke
      ..strokeWidth = width;

    canvas.drawRRect(rrect, paint);
  }

  @override
  bool shouldRepaint(covariant GradientBorderPainter oldDelegate) {
    return oldDelegate.gradient != gradient || oldDelegate.width != width || oldDelegate.radius != radius;
  }
}
