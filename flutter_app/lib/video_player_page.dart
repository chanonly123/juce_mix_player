import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/utils.dart';
import 'package:juce_mix_player/gst_video_player.dart';
import 'package:juce_mix_player/gst_video_view.dart';
import 'package:image_picker/image_picker.dart';
import 'package:path_provider/path_provider.dart';

class VideoPlayerPage extends StatefulWidget {
  const VideoPlayerPage({super.key});

  @override
  VideoPlayerState createState() => VideoPlayerState();
}

class VideoPlayerState extends State<VideoPlayerPage> {
  // --- Controller & State Variables (Kept logic identical) ---
  GstPlayerController player = GstPlayerController();
  double _seekPosition = 0.0;
  double _currentPosition = 0.0;
  double _duration = 0.0;
  bool _isViewReady = false;
  bool _isPlaying = false;
  bool _isMuted = true;
  String _playerState = "IDLE";
  bool _hasVideoLoaded = false;

  // Processing State
  String _currentRotation = "0";
  VisualEffectType _currentEffect = VisualEffectType.none;
  bool _isExporting = false;
  bool _isDragging = false;

  // UI State Variables
  bool _isToolsPanelOpen = false;

  // Trim state
  int _trimStart = 0;
  int _trimEnd = 0;

  @override
  void initState() {
    super.initState();
    _initializePlayer();
  }

  void _initializePlayer() {
    player.setProgressHandler((progress) {
      if (mounted) {
        setState(() {
          if (!_isDragging) {
            _seekPosition = progress;
          }
          _currentPosition = progress * _duration;
        });
      }
    });

    player.setStateUpdateHandler((state) {
      if (mounted) {
        setState(() {
          _playerState = state;
          _isPlaying = player.isPlaying();

          if (state == "READY") {
            _hasVideoLoaded = true;
            double dur = player.getDurationInSecs();
            print("READY $dur");
            _duration = dur;
            _trimStart = 0;
            _trimEnd = (dur * 1000).toInt();
            player.setTrimRange(_trimStart, _trimEnd);
          }

          if (state == "STOPPED" || state == "ERROR" || state == "IDLE") {
            _hasVideoLoaded = false;
            _duration = 0.0;
            _currentPosition = 0.0;
            _seekPosition = 0.0;
          }
        });
      }
    });

    player.setErrorHandler((error) {
      if (mounted) _showSnack('Error: $error', isError: true);
    });
  }

  // --- Logic Helpers ---

  void _showSnack(String msg, {bool isError = false}) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(msg),
        backgroundColor: isError ? Colors.redAccent : Colors.green,
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        margin: const EdgeInsets.all(10),
      ),
    );
  }

  Future<void> pickVideoFromGallery() async {
    final ImagePicker picker = ImagePicker();
    setState(() {
      _hasVideoLoaded = false;
      _isViewReady = false;
    });
    final XFile? video = await picker.pickVideo(source: ImageSource.gallery);
    if (video != null) {
      player.setVideoPath(video.path);
      _showSnack('Video loaded: ${video.name}');
    }
  }

  void toggleMute() {
    player.toggleMute();
    setState(() => _isMuted = player.isMuted());
  }

  void rotateVideo(String degrees) {
    try {
      player.setRotation(degrees);
      setState(() => _currentRotation = degrees);
    } catch (e) {
      _showSnack('Rotation Error: $e', isError: true);
    }
  }

  void applyVisualEffect(VisualEffectType effect) {
    try {
      player.setVisualEffect(effect);
      setState(() => _currentEffect = effect);
    } catch (e) {
      _showSnack('Effect Error: $e', isError: true);
    }
  }

  Future<void> exportVideo() async {
    if (!_hasVideoLoaded) return;
    setState(() => _isExporting = true);
    try {
      final directory = await getApplicationDocumentsDirectory();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      final outputPath = '${directory.path}/exported_video_$timestamp.mp4';
      await player.exportVideo(outputPath);
      _showSnack('Exported to: $outputPath');
    } catch (e) {
      _showSnack('Export failed: $e', isError: true);
    } finally {
      setState(() => _isExporting = false);
    }
  }

  @override
  void dispose() {
    player.dispose();
    super.dispose();
  }

  // --- Widgets ---

  @override
  Widget build(BuildContext context) {
    // Dark theme for the media player look
    return Theme(
      data: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: Colors.black,
        sliderTheme: SliderThemeData(
          trackHeight: 2,
          thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 6),
          overlayShape: const RoundSliderOverlayShape(overlayRadius: 14),
          activeTrackColor: Colors.blueAccent,
          inactiveTrackColor: Colors.white24,
          thumbColor: Colors.white,
        ),
      ),
      child: Scaffold(
        body: SafeArea(
          child: Column(
            children: [
              // 1. The Viewport (Video + Overlays)
              Expanded(
                child: Stack(
                  fit: StackFit.expand,
                  children: [
                    // A. Background & Video
                    Center(
                      child: AspectRatio(
                        aspectRatio: 9 / 16,
                        child: Container(
                          color: Colors.grey[900],
                          child: Stack(
                            children: [
                              GstVideoView(
                                controller: player,
                                onViewReady: () => setState(() => _isViewReady = true),
                              ),
                              // Placeholder / Loading State
                              if (!_hasVideoLoaded && !_isPlaying)
                                Center(
                                  child: Column(
                                    mainAxisSize: MainAxisSize.min,
                                    children: [
                                      Icon(Icons.movie_filter, size: 64, color: Colors.white12),
                                      const SizedBox(height: 16),
                                      const Text("No Video Loaded", style: TextStyle(color: Colors.white38)),
                                    ],
                                  ),
                                ),
                            ],
                          ),
                        ),
                      ),
                    ),

                    // B. Top Bar (Floating)
                    Positioned(
                      top: 0,
                      left: 0,
                      right: 0,
                      child: _buildTopBar(),
                    ),

                    // C. Center Play Button (Overlay)
                    if (_hasVideoLoaded)
                      Center(
                        child: IconButton(
                          iconSize: 80,
                          icon: Icon(
                            _isPlaying ? Icons.pause_circle : Icons.play_circle,
                            color: Colors.white.withOpacity(0.7),
                          ),
                          onPressed: () => player.togglePlayPause(),
                        ),
                      ),

                    // D. Bottom Gradient for text visibility
                    Positioned(
                      bottom: 0,
                      left: 0,
                      right: 0,
                      height: 150,
                      child: Container(
                        decoration: BoxDecoration(
                          gradient: LinearGradient(
                            begin: Alignment.bottomCenter,
                            end: Alignment.topCenter,
                            colors: [Colors.black87, Colors.transparent],
                          ),
                        ),
                      ),
                    ),

                    // E. Primary Controls (Seekbar + Main Actions)
                    Positioned(
                      bottom: 0,
                      left: 0,
                      right: 0,
                      child: _buildPrimaryControls(),
                    ),
                  ],
                ),
              ),

              // 2. The Collapsible Tools Panel
              AnimatedContainer(
                duration: const Duration(milliseconds: 300),
                curve: Curves.easeInOut,
                height: _isToolsPanelOpen ? 240 : 0,
                decoration: BoxDecoration(
                  color: const Color(0xFF1E1E1E),
                  border: Border(top: BorderSide(color: Colors.white10)),
                ),
                child: SingleChildScrollView(
                  physics: const NeverScrollableScrollPhysics(),
                  child: _buildToolsPanel(),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildTopBar() {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: [Colors.black87, Colors.transparent],
        ),
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          // Status Indicator
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
            decoration: BoxDecoration(
              color: Colors.black45,
              borderRadius: BorderRadius.circular(4),
            ),
            child: Row(
              children: [
                Icon(Icons.circle, size: 8, color: _getStateColor(_playerState)),
                const SizedBox(width: 6),
                Text(_playerState, style: const TextStyle(fontSize: 10, fontWeight: FontWeight.bold)),
              ],
            ),
          ),
          Row(
            children: [
              // Load Sample Button
              if (_isViewReady)
                IconButton(
                  icon: const Icon(Icons.smart_display_outlined),
                  tooltip: "Load Sample",
                  onPressed: () async {
                    setState(() {
                      _hasVideoLoaded = false;
                      _isViewReady = false;
                    });
                    final pathL = await AssetHelper.extractAsset('assets/media/sample_3.mp4');
                    player.setVideoPath(pathL);
                  },
                ),
              IconButton(
                icon: Icon(_isMuted ? Icons.volume_off : Icons.volume_up),
                onPressed: toggleMute,
              ),
              IconButton(
                icon: const Icon(Icons.add_photo_alternate_outlined),
                onPressed: pickVideoFromGallery,
              ),
            ],
          )
        ],
      ),
    );
  }

  Widget _buildPrimaryControls() {
    return Padding(
      padding: const EdgeInsets.only(bottom: 16, left: 16, right: 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          // Time & Seek
          Row(
            children: [
              Text(TimeUtils.formatDuration(_currentPosition), style: const TextStyle(fontSize: 12)),
              Expanded(
                child: Slider(
                  value: _seekPosition,
                  onChangeStart: (_) => setState(() => _isDragging = true),
                  onChanged: (v) => setState(() => _seekPosition = v),
                  onChangeEnd: (v) {
                    setState(() => _isDragging = false);
                    player.seek(v);
                  },
                ),
              ),
              Text(TimeUtils.formatDuration(_duration), style: const TextStyle(fontSize: 12)),
            ],
          ),

          // Bottom Action Row
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              // Play/Pause Mini
              IconButton(
                icon: Icon(_isPlaying ? Icons.pause : Icons.play_arrow),
                onPressed: _hasVideoLoaded ? () => player.togglePlayPause() : null,
              ),

              // Toggle Edit Panel Button
              FilledButton.icon(
                onPressed: _hasVideoLoaded
                    ? () {
                        setState(() {
                          _isToolsPanelOpen = !_isToolsPanelOpen;
                        });
                      }
                    : null,
                icon: Icon(_isToolsPanelOpen ? Icons.keyboard_arrow_down : Icons.edit),
                label: Text(_isToolsPanelOpen ? "Close Tools" : "Edit Video"),
                style: FilledButton.styleFrom(
                  backgroundColor: _isToolsPanelOpen ? Colors.grey[800] : Colors.blueAccent,
                  foregroundColor: Colors.white,
                ),
              ),

              // Stop
              IconButton(
                icon: const Icon(Icons.stop),
                onPressed: _hasVideoLoaded ? () => player.stop() : null,
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildToolsPanel() {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // 1. Processing Label
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              const Text("VIDEO TOOLS",
                  style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, letterSpacing: 1.2, color: Colors.grey)),
              if (_isExporting) const SizedBox(height: 14, width: 14, child: CircularProgressIndicator(strokeWidth: 2))
            ],
          ),
          const SizedBox(height: 12),

          // 2. Rotation Row
          Row(
            children: [
              const Icon(Icons.rotate_right, size: 20, color: Colors.grey),
              const SizedBox(width: 12),
              Expanded(
                child: SizedBox(
                  height: 40,
                  child: ListView(
                    scrollDirection: Axis.horizontal,
                    children: ['0', '90', '180', '270'].map((deg) {
                      bool isSelected = _currentRotation == deg;
                      return Padding(
                        padding: const EdgeInsets.only(right: 8),
                        child: ChoiceChip(
                          label: Text('$deg°'),
                          selected: isSelected,
                          onSelected: (s) => rotateVideo(deg),
                          selectedColor: Colors.blueAccent.withOpacity(0.3),
                          labelStyle: TextStyle(color: isSelected ? Colors.blueAccent : Colors.white, fontSize: 12),
                        ),
                      );
                    }).toList(),
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),

          // 3. Effects List (Horizontal)
          SizedBox(
            height: 80,
            child: ListView(
              scrollDirection: Axis.horizontal,
              children: VisualEffectType.values.map((effect) {
                bool isSelected = _currentEffect == effect;
                return GestureDetector(
                  onTap: () => applyVisualEffect(effect),
                  child: Container(
                    width: 70,
                    margin: const EdgeInsets.only(right: 8),
                    decoration: BoxDecoration(
                      color: isSelected ? Colors.purpleAccent.withOpacity(0.2) : Colors.white10,
                      border: Border.all(color: isSelected ? Colors.purpleAccent : Colors.transparent),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(Icons.auto_fix_high, size: 24, color: isSelected ? Colors.purpleAccent : Colors.grey),
                        const SizedBox(height: 4),
                        Text(
                          effect.name.toUpperCase(),
                          textAlign: TextAlign.center,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: TextStyle(fontSize: 10, color: isSelected ? Colors.white : Colors.grey),
                        ),
                      ],
                    ),
                  ),
                );
              }).toList(),
            ),
          ),

          const SizedBox(height: 12),

          // 4. Trim Range Controls
          const Row(
            children: [
              Icon(Icons.content_cut, size: 20, color: Colors.grey),
              SizedBox(width: 12),
              Text('TRIM RANGE', style: TextStyle(fontSize: 11, color: Colors.grey)),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              const Text('Start:', style: TextStyle(fontSize: 11)),
              Expanded(
                child: Slider(
                  value: _trimStart.toDouble(),
                  min: 0.0,
                  max: _duration * 1000.0,
                  onChanged: _hasVideoLoaded
                      ? (v) {
                          setState(() {
                            _trimStart = v.clamp(0.0, _trimEnd).toInt();
                            player.setTrimRange(_trimStart, _trimEnd);
                          });
                        }
                      : null,
                ),
              ),
              Text(TimeUtils.formatDuration(_trimStart / 1000.0), style: const TextStyle(fontSize: 11)),
            ],
          ),
          Row(
            children: [
              const Text('End:', style: TextStyle(fontSize: 11)),
              Expanded(
                child: Slider(
                  value: _trimEnd.toDouble(),
                  min: 0.0,
                  max: _duration * 1000.0,
                  onChanged: _hasVideoLoaded
                      ? (v) {
                          setState(() {
                            _trimEnd = v.clamp(_trimStart, _duration * 1000.0).toInt();
                            player.setTrimRange(_trimStart, _trimEnd);
                          });
                        }
                      : null,
                ),
              ),
              Text(TimeUtils.formatDuration(_trimEnd / 1000.0), style: const TextStyle(fontSize: 11)),
            ],
          ),

          const SizedBox(height: 12),

          // 5. Export Button (Full Width)
          SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              onPressed: _isExporting ? null : exportVideo,
              icon: const Icon(Icons.download_rounded),
              label: Text(_isExporting ? "EXPORTING..." : "EXPORT VIDEO"),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.green[700],
                foregroundColor: Colors.white,
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Color _getStateColor(String state) {
    switch (state.toUpperCase()) {
      case 'PLAYING':
        return Colors.greenAccent;
      case 'PAUSED':
        return Colors.orangeAccent;
      case 'STOPPED':
        return Colors.redAccent;
      case 'ERROR':
        return Colors.red;
      case 'READY':
        return Colors.blueAccent;
      default:
        return Colors.grey;
    }
  }
}
