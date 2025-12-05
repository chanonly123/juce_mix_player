// // Balanced Video Thumbnail Strip (FFmpeg on-demand + sequential preload + LRU cache)
// // -------------------------------------------------------------------------------
// // Place this file in your Flutter project (e.g. lib/widgets/video_thumbnail_strip_ffmpeg.dart)
// // Add to pubspec.yaml:
// //   ffmpeg_kit_flutter_new: ^4.6.0
// //   path_provider: ^2.0.0
// //
// // Features:
// // - Sequential, async, lazy generation of thumbnails (start -> end) so UI isn't blocked.
// // - On-demand preview extraction when user scrubs/drags (throttled).
// // - Memory LRU cache + disk cache under temporary directory.
// // - Configurable thumbnailCount, previewFPS throttle, cache sizes, and more.
// // - Exposes a controller for programmatic seeking.
// //
// // Notes:
// // - FFmpeg extraction is relatively fast but still uses CPU; prefer platform native decoders for maximum efficiency.
// // - Make sure to follow ffmpeg_kit installation instructions for Android/iOS.

// import 'dart:async';
// import 'dart:collection';
// import 'dart:io';

// import 'package:flutter/foundation.dart';
// import 'package:flutter/material.dart';
// import 'package:ffmpeg_kit_flutter_new/ffmpeg_kit.dart';
// import 'package:path_provider/path_provider.dart';

// /// Controller to interact with the thumbnail strip programmatically.
// class ThumbnailStripController {
//   void Function(Duration)? _internalSeek;
//   void Function()? _internalDispose;

//   /// Seek the strip to a given [position]. This will call the widget's [onSeek] callback.
//   void seekTo(Duration position) {
//     _internalSeek?.call(position);
//   }

//   void _setInternalHooks({void Function(Duration)? seek, void Function()? dispose}) {
//     _internalSeek = seek;
//     _internalDispose = dispose;
//   }

//   void dispose() {
//     _internalDispose?.call();
//     _internalSeek = null;
//     _internalDispose = null;
//   }
// }

// class VideoThumbnailStripFfmpeg extends StatefulWidget {
//   final String filePath;
//   final double width;
//   final double height;
//   final int thumbnailCount; // how many thumbnails to lay out across the strip
//   final double spacing;
//   final BorderRadius? borderRadius;
//   final void Function(Duration position)? onSeek; // when user picks a position
//   final Duration videoDuration;
//   final ThumbnailStripController? controller;
//   final Color? backgroundColor;
//   final BoxFit fit;
//   final bool showTimestamp;
//   final bool showPreviewOnSeek; // new feature: show a preview thumbnail above seeker while dragging
//   final int previewThrottleMs; // throttle preview extraction to this ms (e.g. 66 -> ~15fps)
//   final int maxMemoryCache; // how many frames kept in memory

//   const VideoThumbnailStripFfmpeg({
//     Key? key,
//     required this.filePath,
//     this.width = double.infinity,
//     this.height = 64,
//     this.thumbnailCount = 8,
//     this.spacing = 2.0,
//     this.borderRadius,
//     this.onSeek,
//     required this.videoDuration,
//     this.controller,
//     this.backgroundColor,
//     this.fit = BoxFit.cover,
//     this.showTimestamp = false,
//     this.showPreviewOnSeek = true,
//     this.previewThrottleMs = 66,
//     this.maxMemoryCache = 150,
//   })  : assert(thumbnailCount > 0),
//         assert(maxMemoryCache > 0),
//         super(key: key);

//   @override
//   State<VideoThumbnailStripFfmpeg> createState() => _VideoThumbnailStripFfmpegState();
// }

// class _VideoThumbnailStripFfmpegState extends State<VideoThumbnailStripFfmpeg> {
//   Duration? _duration;
//   bool _loading = true;
//   bool _disposed = false;

//   // Simple LRU memory cache (keyed by int frameIndex or special preview key)
//   final LinkedHashMap<String, Uint8List> _memoryCache = LinkedHashMap();

//   // Disk cache directory
//   Directory? _cacheDir;

//   // Current seeker fraction (0.0 - 1.0)
//   double _dragFraction = 0.0;

//   // Sequential preload state
//   bool _preloading = false;
//   // int _preloadIndex = 0;

//   // Throttling preview extraction
//   DateTime _lastPreviewAt = DateTime.fromMillisecondsSinceEpoch(0);

//   // Preview bytes to show above seeker while dragging
//   Uint8List? _previewBytes;

//   // A small mutex to avoid overlapping preview generations
//   bool _previewInProgress = false;

//   @override
//   void initState() {
//     super.initState();
//     _duration = widget.videoDuration;
//     widget.controller?._setInternalHooks(seek: _programmaticSeek, dispose: _controllerDispose);
//     _init();
//   }

//   Future<void> _init() async {
//     _cacheDir = await _makeCacheDir();
//     // begin lazy sequential generation (non-blocking)
//     _startSequentialPreload();
//     if (!_disposed) setState(() => _loading = false);
//   }

//   Future<Directory> _makeCacheDir() async {
//     final tmp = await getTemporaryDirectory();
//     final videoHash = widget.filePath.hashCode.toUnsigned(32).toString();
//     final dir = Directory('${tmp.path}/video_thumb_cache_$videoHash');
//     if (!await dir.exists()) await dir.create(recursive: true);
//     return dir;
//   }

//   String _diskThumbPathForIndex(int index) => '${_cacheDir!.path}/thumb_index_$index.jpg';
//   String _diskThumbPathForPreview(int timeMs) => '${_cacheDir!.path}/thumb_preview_${timeMs}.jpg';

//   Future<void> _startSequentialPreload() async {
//     if (_preloading || _duration == null) return;
//     _preloading = true;
//     final totalMs = _duration!.inMilliseconds;

//     for (var i = 0; i < widget.thumbnailCount; i++) {
//       if (_disposed) break;
//       // If already in cache skip
//       final key = 'i_$i';
//       if (_memoryCache.containsKey(key)) continue;

//       final timeMs = (totalMs * (i / (widget.thumbnailCount - 1))).round();
//       final diskPath = _diskThumbPathForIndex(i);

//       // If disk file present, load to memory cache
//       final f = File(diskPath);
//       if (await f.exists()) {
//         final bytes = await f.readAsBytes();
//         _addToMemoryCache(key, bytes);
//         if (!_disposed) setState(() {});
//         continue;
//       }

//       // Generate via ffmpeg and write to disk
//       await _generateFrameToDisk(timeMs, diskPath);
//       if (_disposed) break;
//       if (await File(diskPath).exists()) {
//         final bytes = await File(diskPath).readAsBytes();
//         _addToMemoryCache(key, bytes);
//         if (!_disposed) setState(() {});
//       }

//       // Small delay to yield to UI and avoid saturating CPU
//       await Future.delayed(const Duration(milliseconds: 40));
//     }

//     _preloading = false;
//   }

//   Future<void> _generateFrameToDisk(int timeMs, String outPath) async {
//     // Time in seconds with millisecond precision
//     final pos = (timeMs / 1000.0).toStringAsFixed(3);
//     final quotedIn = '"${widget.filePath.replaceAll('"', '\\"')}"';
//     final quotedOut = '"${outPath.replaceAll('"', '\\"')}"';

//     final cmd = '-ss $pos -i $quotedIn -frames:v 1 -q:v 2 -y $quotedOut';

//     try {
//       await FFmpegKit.execute(cmd);
//     } catch (e) {
//       // ignore, generation failed
//       debugPrint('FFmpeg generate error: $e');
//     }
//   }

//   // Add to LRU memory cache, evicting oldest if > maxMemoryCache
//   void _addToMemoryCache(String key, Uint8List bytes) {
//     if (_memoryCache.containsKey(key)) {
//       final existing = _memoryCache.remove(key);
//       // re-insert to mark as most-recent
//       if (existing != null) _memoryCache[key] = existing;
//       return;
//     }
//     _memoryCache[key] = bytes;
//     while (_memoryCache.length > widget.maxMemoryCache) {
//       _memoryCache.remove(_memoryCache.keys.first);
//     }
//   }

//   // Try to get thumbnail by index from memory/disk
//   Future<Uint8List?> _getThumbByIndex(int index) async {
//     final key = 'i_$index';
//     if (_memoryCache.containsKey(key)) return _memoryCache[key];
//     final disk = File(_diskThumbPathForIndex(index));
//     if (await disk.exists()) {
//       final bytes = await disk.readAsBytes();
//       _addToMemoryCache(key, bytes);
//       return bytes;
//     }
//     return null;
//   }

//   // On-demand preview for a specific time (ms). This will check cache and disk first, otherwise generate.
//   Future<Uint8List?> _getPreviewAtMs(int timeMs) async {
//     final key = 'p_$timeMs';
//     if (_memoryCache.containsKey(key)) return _memoryCache[key];

//     final diskPath = _diskThumbPathForPreview(timeMs);
//     final diskFile = File(diskPath);
//     if (await diskFile.exists()) {
//       final bytes = await diskFile.readAsBytes();
//       _addToMemoryCache(key, bytes);
//       return bytes;
//     }

//     // Not present -> generate one to disk
//     if (_previewInProgress) return null;
//     _previewInProgress = true;
//     await _generateFrameToDisk(timeMs, diskPath);
//     _previewInProgress = false;

//     if (await diskFile.exists()) {
//       final bytes = await diskFile.readAsBytes();
//       _addToMemoryCache(key, bytes);
//       return bytes;
//     }
//     return null;
//   }

//   void _controllerDispose() {
//     _disposed = true;
//     _memoryCache.clear();
//   }

//   void _programmaticSeek(Duration position) {
//     if (_duration == null) return;
//     final frac = (position.inMilliseconds / _duration!.inMilliseconds).clamp(0.0, 1.0);
//     setState(() {
//       _dragFraction = frac;
//     });
//   }

//   @override
//   void dispose() {
//     _disposed = true;
//     widget.controller?._setInternalHooks(seek: null, dispose: null);
//     super.dispose();
//   }

//   void _reportSeekByFraction(double frac) {
//     if (_duration == null) return;
//     final millis = (_duration!.inMilliseconds * frac).round();
//     final pos = Duration(milliseconds: millis);
//     widget.onSeek?.call(pos);
//   }

//   String _formatDuration(Duration d) {
//     final totalSeconds = d.inSeconds;
//     final minutes = (totalSeconds ~/ 60).toString().padLeft(2, '0');
//     final seconds = (totalSeconds % 60).toString().padLeft(2, '0');
//     return '$minutes:$seconds';
//   }

//   @override
//   Widget build(BuildContext context) {
//     return LayoutBuilder(builder: (context, constraints) {
//       final containerWidth = widget.width.isFinite ? widget.width : constraints.maxWidth;

//       return SizedBox(
//         width: widget.width.isFinite ? widget.width : double.infinity,
//         height: widget.height + (widget.showPreviewOnSeek ? (widget.height * 1.5) : 0),
//         child: ClipRRect(
//           borderRadius: widget.borderRadius ?? BorderRadius.circular(6),
//           child: Container(
//             color: widget.backgroundColor ?? Colors.black12,
//             child: _loading
//                 ? Center(child: SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2)))
//                 : GestureDetector(
//                     behavior: HitTestBehavior.opaque,
//                     onHorizontalDragStart: (details) {
//                       final local = (context.findRenderObject() as RenderBox).globalToLocal(details.globalPosition);
//                       _handleDragAt(local.dx, containerWidth);
//                     },
//                     onHorizontalDragUpdate: (details) {
//                       final local = (context.findRenderObject() as RenderBox).globalToLocal(details.globalPosition);
//                       _handleDragAt(local.dx, containerWidth);
//                     },
//                     onHorizontalDragEnd: (details) {
//                       // when drag ends, optionally clear preview
//                       if (widget.showPreviewOnSeek) {
//                         setState(() {
//                           _previewBytes = null;
//                         });
//                       }
//                     },
//                     onTapDown: (details) {
//                       final local = (context.findRenderObject() as RenderBox).globalToLocal(details.globalPosition);
//                       _handleDragAt(local.dx, containerWidth);
//                     },
//                     child: Stack(fit: StackFit.expand, children: [
//                       _buildThumbnailsRow(containerWidth),
//                       Positioned(
//                         left: (_dragFraction * containerWidth) - 1,
//                         top: widget.showPreviewOnSeek ? widget.height * 0.9 : 0,
//                         bottom: 0,
//                         child: Container(width: 2, color: Colors.white),
//                       ),

//                       // preview box above the strip
//                       if (widget.showPreviewOnSeek && _previewBytes != null)
//                         Positioned(
//                           left: (_dragFraction * containerWidth) - (widget.height * 0.45),
//                           top: 0,
//                           child: Container(
//                             width: widget.height * 0.9,
//                             height: widget.height * 0.9,
//                             decoration: BoxDecoration(boxShadow: [BoxShadow(color: Colors.black26, blurRadius: 6)]),
//                             child: Image.memory(
//                               _previewBytes!,
//                               fit: widget.fit,
//                             ),
//                           ),
//                         ),

//                       if (widget.showTimestamp && _duration != null)
//                         Positioned(
//                           left: (_dragFraction * containerWidth) - 40,
//                           bottom: 2,
//                           child: Container(
//                             padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
//                             decoration: BoxDecoration(color: Colors.black54, borderRadius: BorderRadius.circular(4)),
//                             child: Text(
//                               _formatDuration(
//                                   Duration(milliseconds: (_duration!.inMilliseconds * _dragFraction).round())),
//                               style: const TextStyle(color: Colors.white, fontSize: 12),
//                             ),
//                           ),
//                         ),
//                     ]),
//                   ),
//           ),
//         ),
//       );
//     });
//   }

//   Widget _buildThumbnailsRow(double containerWidth) {
//     final count = widget.thumbnailCount;
//     final thumbWidth = (containerWidth - (widget.spacing * (count - 1))) / count;

//     final thumbs = List<Widget>.generate(count, (i) {
//       final key = 'i_$i';
//       final data = _memoryCache.containsKey(key) ? _memoryCache[key] : null;

//       final child = data != null
//           ? Image.memory(data, fit: widget.fit, width: thumbWidth, height: widget.height)
//           : Container(
//               width: thumbWidth,
//               height: widget.height,
//               color: Colors.black26,
//               child: const Center(child: Icon(Icons.videocam)));

//       return GestureDetector(
//         onTap: () async {
//           final frac = (i / (widget.thumbnailCount - 1)).clamp(0.0, 1.0);
//           setState(() {
//             _dragFraction = frac;
//           });
//           _reportSeekByFraction(frac);
//         },
//         child: Padding(
//           padding: EdgeInsets.only(right: i == count - 1 ? 0 : widget.spacing),
//           child: child,
//         ),
//       );
//     });

//     return Row(crossAxisAlignment: CrossAxisAlignment.stretch, children: thumbs);
//   }

//   void _handleDragAt(double dx, double containerWidth) {
//     final frac = (dx / containerWidth).clamp(0.0, 1.0);
//     setState(() {
//       _dragFraction = frac;
//     });
//     _reportSeekByFraction(frac);

//     if (!widget.showPreviewOnSeek || _duration == null) return;

//     final now = DateTime.now();
//     if (now.difference(_lastPreviewAt).inMilliseconds < widget.previewThrottleMs) return;
//     _lastPreviewAt = now;

//     final millis = (_duration!.inMilliseconds * frac).round();

//     final approxIndex = ((widget.thumbnailCount - 1) * frac).round().clamp(0, widget.thumbnailCount - 1);
//     _getThumbByIndex(approxIndex).then((firstBytes) async {
//       if (_disposed) return;

//       if (firstBytes != null) {
//         setState(() {
//           _previewBytes = firstBytes;
//         });
//         return;
//       }

//       final previewBytes = await _getPreviewAtMs(millis);
//       if (_disposed) return;

//       if (previewBytes != null) {
//         setState(() {
//           _previewBytes = previewBytes;
//         });
//       }
//     });
//   }
// }
