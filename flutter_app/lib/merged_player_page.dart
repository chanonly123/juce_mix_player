import 'dart:async';
import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/utils.dart';
import 'package:juce_mix_player/gst_video_player.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:juce_mix_player/unified_av_player.dart';
import 'package:juce_mix_player/unified_video_view.dart';
import 'package:juce_mix_player/video_thumbnail_strip.dart';
import 'package:wechat_assets_picker/wechat_assets_picker.dart';

const LinearGradient gradientPurpleBorder = LinearGradient(
  begin: Alignment.topCenter,
  end: Alignment.bottomCenter,
  colors: [
    Color(0xFFF74BF7),
    Color(0xFF080C28),
    Color(0xFF2FE4F9),
  ],
  stops: [0.0, 0.515, 1.0],
);

class MergedPlayerPage extends StatefulWidget {
  const MergedPlayerPage({super.key});

  @override
  MergedPlayerPageState createState() => MergedPlayerPageState();
}

class MergedPlayerPageState extends State<MergedPlayerPage> {
  late UnifiedAVPlayerController player;

  bool isDiscarding = false;
  String? currentVideoPath;

  double progress = 0.0;
  bool isSliderEditing = false;
  bool isPlaying = false;
  JuceMixPlayerState state = JuceMixPlayerState.IDLE;
  bool hasVideoLoaded = false;
  bool hasVideoReady = false;

  double bgmVolume = 0.7;
  double vocalVolume = 1.0;
  bool guideEnabled = false;
  bool metronomeEnabled = false;
  MixerComposeModel? lastMixerComposeModel;

  double guideVolume = 1.0;
  double metronomeVolume = 1.0;
  int masterDurationMs = 0;

  // User-facing values (original video, no padding)
  int userVideoDurationMs = 0; // Original video duration WITHOUT padding
  int userTrimStartMs = 0; // User sees this as trim start (0-based)
  int userTrimEndMs = 0; // User sees this as trim end

  // Internal values (with padding)
  int internalVideoDurationMs = 0; // Video duration WITH front padding
  int internalTrimStartMs = 0; // Actual trim start sent to player
  int internalTrimEndMs = 0; // Actual trim end sent to player

  // Latency adjustment
  int latencyAdjustmentMs = 0; // Current latency offset (-maxLatencyMs to +maxLatencyMs)

  // Constants
  static const int maxLatencyMs = 1000;
  static const int latencyStepMs = 5;

  int currentRotation = 0;
  VideoFlipMethod currentFlipMethod = VideoFlipMethod.none;
  VisualEffectType currentEffect = VisualEffectType.none;
  bool isExporting = false;

  bool isAudioPanelExpanded = true;
  bool isVideoPanelExpanded = false;

  bool hasAudioLoaded = false;
  bool isOverlayVisible = true;
  Timer? _overlayTimer;

  Map<String, bool> showVolumeBars = {
    'bgm': false,
    'vocal': false,
    'guide': false,
    'metronome': false,
  };

  final Map<String, LayerLink> _layerLinks = {
    'bgm': LayerLink(),
    'vocal': LayerLink(),
    'guide': LayerLink(),
    'metronome': LayerLink(),
  };

  @override
  void initState() {
    super.initState();
    player = UnifiedAVPlayerController();
    _initializePlayer();
  }

  void _initializePlayer() {
    player.setProgressHandler((progress) {
      if (!isSliderEditing && mounted) {
        setState(() => this.progress = progress);
      }
    });

    player.setStateUpdateHandler((newState) {
      if (mounted) {
        setState(() {
          state = newState;
          if (newState == JuceMixPlayerState.READY) {
            masterDurationMs = (player.getDuration() * 1000).toInt();
          }
          isPlaying = newState == JuceMixPlayerState.PLAYING;
        });
      }
    });

    player.setErrorHandler((error) {
      if (mounted) {
        _showSnack('Error: $error', isError: true);
      }
    });

    player.setVideoStateUpdateHandler((videoState) {
      print('Video state update: $videoState');
      if (mounted) {
        if (videoState == 'READY') {
          setState(() {
            hasVideoReady = true;
            internalVideoDurationMs = (player.getVideoDuration() * 1000).toInt();
            userVideoDurationMs = internalVideoDurationMs - maxLatencyMs;
          });
          _updateTrimRangeInitial();
          _debugPrintTrimState('After video ready');
        }
      }
    });

    // player.setVideoProgressHandler((videoProgress) {});

    player.setAudioSettings(MixerSettings(
      progressUpdateInterval: 0.05,
    ));

    // Initialize padding
    player.setVideoPadding(maxLatencyMs, true);
  }

  void _showSnack(String msg, {bool isError = false, bool isSuccess = false}) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        duration: Duration(milliseconds: 500),
        content: Text(msg),
        backgroundColor: isError
            ? Colors.redAccent
            : isSuccess
                ? Colors.green
                : Colors.blueGrey,
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        margin: const EdgeInsets.all(10),
      ),
    );
  }

  Future<void> _loadSampleAudio() async {
    final path = await AssetHelper.extractAsset('assets/media/Fate_of_Ophelia.flac');
    await _createComposeModel(path);
    setState(() => hasAudioLoaded = true);
    _showSnack('Sample audio loaded', isSuccess: true);
  }

  Future<void> _loadAudioFromGallery() async {
    try {
      FilePickerResult? result = await FilePicker.platform.pickFiles(
        type: FileType.custom,
        allowedExtensions: ['mp3', 'flac', 'wav'],
        allowMultiple: false,
      );

      if (result != null && result.files.single.path != null) {
        final filePath = result.files.single.path!;
        final fileName = result.files.single.name;
        await _createComposeModel(filePath);
        setState(() => hasAudioLoaded = true);
        _showSnack('Audio loaded: $fileName', isSuccess: true);
      } else {
        _showSnack('No file selected');
      }
    } catch (e) {
      _showSnack('Error loading audio: $e', isError: true);
    }
  }

  Future<void> _loadSampleVideo() async {
    setState(() {
      hasVideoLoaded = false;
    });
    try {
      final path = await AssetHelper.extractAsset('assets/media/Fate_of_Ophelia_muted.mp4');
      player.setVideoPath(path);
      setState(() {
        hasVideoLoaded = true;
      });
      currentVideoPath = path;
    } catch (e) {
      _showSnack('Error loading video: $e', isError: true);
    }
  }

  // Future<void> _loadVideoFromGallery() async {
  //   final ImagePicker picker = ImagePicker();
  //   setState(() {
  //     hasVideoLoaded = false;
  //   });
  //   final XFile? video = await picker.pickVideo(source: ImageSource.gallery);
  //   if (video != null) {
  //     try {
  //       player.setVideoPath(video.path);
  //       setState(() {
  //         hasVideoLoaded = true;
  //       });
  //       currentVideoPath = video.path;
  //     } catch (e) {
  //       _showSnack('Error loading video: $e', isError: true);
  //     }
  //   }
  // }

  Future<void> _loadVideoFromGallery() async {
    // 1. Pick the asset (This opens a WhatsApp-style gallery)
    // This is nearly instant because it just reads metadata first.
    final List<AssetEntity>? result = await AssetPicker.pickAssets(
      context,
      pickerConfig: const AssetPickerConfig(
        requestType: RequestType.video,
        maxAssets: 1, // Limit to 1 video like your original code
      ),
    );

    if (result != null && result.isNotEmpty) {
      setState(() {
        hasVideoLoaded = false; // Show loading while we fetch the actual file
      });

      final AssetEntity videoAsset = result.first;

      // 2. Get the file.
      // photo_manager attempts to return the original file without copying
      // if permissions allow, making this much faster than image_picker.
      final File? videoFile = await videoAsset.file;

      if (videoFile != null) {
        try {
          // 3. Initialize Player
          // Note: For even faster start, some advanced players play directly
          // from the AssetEntity, but standard VideoPlayer needs a file/path.
          player.setVideoPath(videoFile.path);

          setState(() {
            hasVideoLoaded = true;
            currentVideoPath = videoFile.path;
          });
        } catch (e) {
          _showSnack('Error loading video: $e', isError: true);
        }
      }
    }
  }

  Future<void> _createComposeModel(String bgmPath) async {
    final pathH = await AssetHelper.extractAsset('assets/media/met_h.wav');
    final pathL = await AssetHelper.extractAsset('assets/media/met_l.wav');
    lastMixerComposeModel = MixerComposeModel(
      tracks: [
        MixerTrack(id: "bgm", path: bgmPath, volume: bgmVolume, enabled: true),
        MixerTrack(
          id: "metronome_track_0",
          path: pathH,
          offset: 0,
          volume: bgmVolume,
          enabled: metronomeEnabled,
          repeat: true,
          repeatInterval: 3.2,
        ),
        MixerTrack(
          id: "metronome_track_1",
          path: pathL,
          offset: 0.8,
          volume: bgmVolume,
          enabled: metronomeEnabled,
          repeat: true,
          repeatInterval: 3.2,
        ),
        MixerTrack(
          id: "metronome_track_2",
          path: pathL,
          offset: 1.6,
          volume: bgmVolume,
          enabled: metronomeEnabled,
          repeat: true,
          repeatInterval: 3.2,
        ),
        MixerTrack(
          id: "metronome_track_3",
          path: pathL,
          offset: 2.4,
          volume: bgmVolume,
          enabled: metronomeEnabled,
          repeat: true,
          repeatInterval: 3.2,
        ),
      ],
    );
    player.setAudioData(lastMixerComposeModel!);
  }

  void _updateAudioMix() async {
    if (lastMixerComposeModel == null) return;
    lastMixerComposeModel = lastMixerComposeModel!.copyWith(
      tracks: lastMixerComposeModel!.tracks?.map((track) {
        if (track.id == 'bgm') {
          return track.copyWith(volume: bgmVolume, enabled: true);
        } else if (track.id.startsWith('metronome')) {
          return track.copyWith(volume: metronomeVolume, enabled: metronomeEnabled);
        } else {
          return track.copyWith(volume: bgmVolume, enabled: metronomeEnabled);
        }
      }).toList(),
    );
    player.setAudioData(lastMixerComposeModel!);
  }

  void _updateTrimRangeInitial() {
    final maxEndMs = userVideoDurationMs < masterDurationMs ? userVideoDurationMs : masterDurationMs;

    setState(() {
      userTrimStartMs = 0;
      userTrimEndMs = maxEndMs;
      latencyAdjustmentMs = 0;
    });

    _updateInternalTrimValues();
    _debugPrintTrimState('Initial trim range');
  }

  void _updateInternalTrimValues() {
    // Base conversion: user coordinates -> internal coordinates
    int proposedStart = VideoCoordinateUtils.userToInternal(userTrimStartMs, maxLatencyMs, latencyAdjustmentMs);
    int proposedEnd = VideoCoordinateUtils.userToInternal(userTrimEndMs, maxLatencyMs, latencyAdjustmentMs);

    // Business Rule 1: If at start and latency is negative, shift start into padding
    // (increases trim duration instead of sliding window)
    if (userTrimStartMs == 0 && latencyAdjustmentMs < 0) {
      proposedStart = maxLatencyMs + latencyAdjustmentMs;
      proposedEnd = userTrimEndMs + maxLatencyMs; // Only add padding, NOT latency
    }

    // Business Rule 2: If at end and latency is positive, shift start right
    // (decreases trim duration instead of sliding window)
    if (userTrimEndMs >= userVideoDurationMs && latencyAdjustmentMs > 0) {
      proposedEnd = internalVideoDurationMs;
      proposedStart = userTrimStartMs + maxLatencyMs + latencyAdjustmentMs;
    }

    // Clamp to valid range
    internalTrimStartMs = VideoCoordinateUtils.clampMs(proposedStart, 0, internalVideoDurationMs);
    internalTrimEndMs = VideoCoordinateUtils.clampMs(proposedEnd, internalTrimStartMs, internalVideoDurationMs);

    // Send to player
    player.setVideoTrimRange(internalTrimStartMs, internalTrimEndMs);

    _debugPrintTrimState('Update internal values');
  }

  @override
  void dispose() {
    if (player.isPlaying()) {
      player.pause();
    }

    Future.delayed(const Duration(milliseconds: 1500), () {
      player.dispose();
    });

    _overlayTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Theme(
      data: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: const Color(0xFF121212),
        sliderTheme: SliderThemeData(
          trackHeight: 3,
          thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 7),
          overlayShape: const RoundSliderOverlayShape(overlayRadius: 16),
          activeTrackColor: Colors.cyanAccent,
          inactiveTrackColor: Colors.white24,
          thumbColor: Colors.white,
        ),
      ),
      child: Scaffold(
        body: isDiscarding
            ? Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    CircularProgressIndicator(color: Colors.redAccent),
                    SizedBox(height: 16),
                    Text("Discarding...", style: TextStyle(color: Colors.white)),
                  ],
                ),
              )
            : hasAudioLoaded
                ? _buildVideoPlayerScreen()
                : _buildAudioLoadScreen(),
      ),
    );
  }

  Widget _buildAudioLoadScreen() {
    return Container(
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [
            Colors.deepPurple.shade900.withValues(alpha: 0.3),
            Colors.cyan.shade900.withValues(alpha: 0.3),
          ],
        ),
      ),
      child: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.audiotrack, size: 80, color: Colors.white30),
            const SizedBox(height: 20),
            Text(
              'Audio Player',
              style: TextStyle(
                fontSize: 24,
                fontWeight: FontWeight.bold,
                color: Colors.white70,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'Load audio to begin',
              style: TextStyle(color: Colors.white54),
            ),
            const SizedBox(height: 48),
            SizedBox(
              width: 250,
              child: ElevatedButton.icon(
                onPressed: _loadSampleAudio,
                icon: const Icon(Icons.music_note),
                label: const Text('Load Sample Audio'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.blueAccent,
                  padding: const EdgeInsets.symmetric(vertical: 16),
                ),
              ),
            ),
            const SizedBox(height: 16),
            SizedBox(
              width: 250,
              child: ElevatedButton.icon(
                onPressed: _loadAudioFromGallery,
                icon: const Icon(Icons.folder_open),
                label: const Text('Load from Gallery'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.white10,
                  foregroundColor: Colors.white,
                  padding: const EdgeInsets.symmetric(vertical: 16),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildVideoPlayerScreen() {
    return Stack(children: [
      Column(children: [
        Expanded(
          child: Stack(
            fit: StackFit.expand,
            children: [
              Container(
                color: Colors.black,
                child: Center(
                  child: AspectRatio(
                    aspectRatio: 9 / 16,
                    child: Container(
                      color: Colors.grey[900],
                      child: hasVideoLoaded ? _buildVideoView() : _buildVideoPlaceholder(),
                    ),
                  ),
                ),
              ),
              Positioned.fill(
                child: GestureDetector(
                  behavior: HitTestBehavior.translucent,
                  onTap: _toggleOverlay,
                  child: AnimatedOpacity(
                    opacity: isOverlayVisible ? 1.0 : 0.0,
                    duration: const Duration(milliseconds: 300),
                    child: Container(
                      color: Colors.black26,
                      child: Center(
                        child: Row(
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            IconButton(
                              iconSize: 64,
                              icon: Icon(
                                isPlaying ? Icons.pause_circle_filled : Icons.play_circle_filled,
                                color: Colors.white.withValues(alpha: 0.8),
                              ),
                              onPressed: () {
                                player.togglePlayPause();
                                _resetOverlayTimer();
                              },
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
                ),
              ),
              Positioned(
                bottom: 16,
                left: 16,
                right: 16,
                child: Row(
                  children: [
                    Text(
                      TimeUtils.formatDurationMs(progress * masterDurationMs),
                      style: const TextStyle(color: Colors.white, fontSize: 12),
                    ),
                    Expanded(
                      child: Slider(
                        value: progress,
                        onChanged: (value) {
                          setState(() => progress = value);
                          _resetOverlayTimer();
                        },
                        onChangeStart: (_) => isSliderEditing = true,
                        onChangeEnd: (value) {
                          isSliderEditing = false;
                          player.seek(value);
                          _resetOverlayTimer();
                        },
                        activeColor: Colors.cyanAccent,
                        inactiveColor: Colors.white24,
                      ),
                    ),
                    Text(
                      TimeUtils.formatDurationMs(masterDurationMs.toDouble()),
                      style: const TextStyle(color: Colors.white, fontSize: 12),
                    ),
                  ],
                ),
              ),
              if (hasVideoReady)
                Positioned(
                  bottom: 60,
                  left: 16,
                  right: 16,
                  child: Container(
                    padding: const EdgeInsets.all(8),
                    decoration: BoxDecoration(
                      color: Colors.black54,
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: VideoThumbnailStrip(
                      filePath: currentVideoPath!,
                      videoDuration: Duration(milliseconds: userVideoDurationMs),
                      maxTrimDurationMs: userVideoDurationMs,
                      initialStartMs: userTrimStartMs,
                      initialEndMs: userTrimEndMs,
                      windowGradient: gradientPurpleBorder,
                      currentPositionMs: _calculateCurrentUserPosition(),
                      onTrimChangeEnd: (TrimData trimData) {
                        print('Trim change from user: Start ${trimData.startMs}ms, End ${trimData.endMs}ms');
                        setState(() {
                          // Reset latency when user manually adjusts trim
                          latencyAdjustmentMs = 0;
                          // Update user values (trimData is already in user coordinates)
                          userTrimStartMs = trimData.startMs;
                          userTrimEndMs = trimData.endMs;
                        });
                        _updateInternalTrimValues();
                      },
                    ),
                  ),
                ),
              Positioned(
                top: MediaQuery.of(context).padding.top + 16,
                left: 16,
                right: 16,
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    IconButton(
                      icon: const Icon(Icons.remove_circle, color: Colors.red),
                      onPressed: _discardPage,
                      tooltip: 'Discard & Reload',
                    ),
                    hasVideoLoaded ? _buildLatencyAdjustmentWidget() : const SizedBox.shrink(),
                    IconButton(
                      icon: isExporting
                          ? SizedBox(
                              width: 24,
                              height: 24,
                              child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2),
                            )
                          : const Icon(Icons.download, color: Colors.white),
                      onPressed: isExporting ? null : _exportVideo,
                      tooltip: 'Export Video',
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
        Container(
          color: const Color(0xFF1E1E1E),
          padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 8),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                children: [
                  _buildTrackControl('BGM', 'bgm', Icons.music_note, bgmVolume),
                  _buildTrackControl('Vocal', 'vocal', Icons.mic, vocalVolume),
                  _buildTrackControl('Guide', 'guide', Icons.headphones, guideVolume,
                      isToggle: true, isEnabled: guideEnabled),
                  _buildTrackControl('Metronome', 'metronome', Icons.timer, metronomeVolume,
                      isToggle: true, isEnabled: metronomeEnabled),
                ],
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  IconButton(
                    onPressed: () {
                      setState(() {
                        hasVideoLoaded = false;
                        currentVideoPath = null;
                      });
                    },
                    icon: const Icon(Icons.videocam_off_outlined, color: Colors.redAccent),
                    tooltip: 'Remove Video',
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: hasVideoLoaded ? _showVideoSettings : null,
                      icon: const Icon(Icons.tune),
                      label: const Text('Video Settings'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.white10,
                        foregroundColor: Colors.white,
                        disabledBackgroundColor: Colors.white10.withValues(alpha: 0.5),
                        disabledForegroundColor: Colors.white30,
                        padding: const EdgeInsets.symmetric(vertical: 12),
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  IconButton(
                    onPressed: _changeVideo,
                    icon:
                        Icon(hasVideoLoaded ? Icons.video_library : Icons.add_circle_outline, color: Colors.cyanAccent),
                    tooltip: hasVideoLoaded ? 'Replace Video' : 'Add Video',
                  ),
                ],
              ),
            ],
          ),
        )
      ]),
      if (showVolumeBars.containsValue(true))
        Positioned.fill(
          child: GestureDetector(
            behavior: HitTestBehavior.translucent,
            onTap: () {
              setState(() {
                showVolumeBars.updateAll((key, value) => false);
              });
            },
            child: Stack(
              children: showVolumeBars.entries.where((e) => e.value).map((e) {
                final id = e.key;
                return CompositedTransformFollower(
                  link: _layerLinks[id]!,
                  offset: const Offset(-15, -150),
                  child: Container(
                    height: 140,
                    width: 50,
                    padding: const EdgeInsets.symmetric(vertical: 8),
                    decoration: BoxDecoration(
                      color: const Color(0xFF2A2A2A),
                      borderRadius: BorderRadius.circular(25),
                      boxShadow: [
                        BoxShadow(color: Colors.black54, blurRadius: 8, offset: Offset(0, 4)),
                      ],
                    ),
                    child: RotatedBox(
                      quarterTurns: 3,
                      child: SliderTheme(
                        data: SliderTheme.of(context).copyWith(
                          trackHeight: 4,
                          thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 8),
                          overlayShape: const RoundSliderOverlayShape(overlayRadius: 16),
                        ),
                        child: Slider(
                          value: _getVolumeForId(id),
                          onChanged: (v) => _updateTrackVolume(id, v, shouldUpdateMix: false),
                          onChangeEnd: (v) => _updateTrackVolume(id, v),
                          activeColor: Colors.cyanAccent,
                          inactiveColor: Colors.grey[800],
                        ),
                      ),
                    ),
                  ),
                );
              }).toList(),
            ),
          ),
        ),
    ]);
  }

  void _showVideoSettings() {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      barrierColor: Colors.transparent,
      backgroundColor: const Color(0xFF1E1E1E),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(16)),
      ),
      builder: (context) {
        return DraggableScrollableSheet(
          expand: false,
          initialChildSize: 0.4,
          minChildSize: 0.3,
          maxChildSize: 0.7,
          builder: (context, scrollController) {
            return StatefulBuilder(builder: (context, setModalState) {
              return SafeArea(
                top: false,
                child: SingleChildScrollView(
                  controller: scrollController,
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: const [
                          Icon(Icons.auto_awesome, size: 16, color: Colors.white70),
                          SizedBox(width: 4),
                          Text('FX',
                              style: TextStyle(fontSize: 12, fontWeight: FontWeight.w500, color: Colors.white70)),
                        ],
                      ),
                      const SizedBox(height: 6),
                      Wrap(
                        spacing: 6,
                        runSpacing: 4,
                        children: VisualEffectType.values.map((effect) {
                          final isSelected = currentEffect == effect;
                          final label = effect.name.toUpperCase();
                          return ChoiceChip(
                            label: Text(label),
                            labelPadding: const EdgeInsets.symmetric(horizontal: 8),
                            materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
                            visualDensity: VisualDensity.compact,
                            selected: isSelected,
                            onSelected: (selected) {
                              if (selected) {
                                setModalState(() => currentEffect = effect);
                                setState(() => currentEffect = effect);
                                player.setVideoVisualEffect(effect.index);
                              }
                            },
                            selectedColor: Colors.black,
                            backgroundColor: Colors.grey[800],
                            labelStyle: TextStyle(
                              color: isSelected ? Colors.white : Colors.white70,
                              fontSize: 11,
                            ),
                          );
                        }).toList(),
                      ),
                      const Divider(color: Colors.white24),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Row(
                            children: const [
                              Icon(Icons.screen_rotation, size: 8, color: Colors.white70),
                              SizedBox(width: 2),
                              Text('ROTATE',
                                  style: TextStyle(fontSize: 12, fontWeight: FontWeight.w500, color: Colors.white70)),
                            ],
                          ),
                          Text(
                            '${currentRotation.clamp(0, 359)}°',
                            style: const TextStyle(color: Colors.white70, fontSize: 12),
                          ),
                        ],
                      ),
                      const SizedBox(height: 6),
                      Slider(
                        min: 0,
                        max: 359,
                        divisions: 359,
                        value: currentRotation.toDouble().clamp(0.0, 359.0),
                        label: '${currentRotation.clamp(0, 359)}°',
                        onChanged: (value) {
                          final angle = value.round().clamp(0, 359).toInt();
                          setModalState(() => currentRotation = angle);
                          setState(() => currentRotation = angle);
                          player.setVideoRotation(angle);
                        },
                      ),
                      const SizedBox(height: 2),
                      Wrap(
                        spacing: 6,
                        children: [0, 90, 180, 270].map((angle) {
                          final isSelected = currentRotation == angle;
                          return ChoiceChip(
                            label: Text('${angle}°'),
                            labelPadding: const EdgeInsets.symmetric(horizontal: 8),
                            materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
                            visualDensity: VisualDensity.compact,
                            selected: isSelected,
                            onSelected: (selected) {
                              if (!selected) return;
                              setModalState(() => currentRotation = angle);
                              setState(() => currentRotation = angle);
                              player.setVideoRotation(angle);
                            },
                            selectedColor: Colors.black,
                            backgroundColor: Colors.grey[800],
                            labelStyle: TextStyle(
                              color: isSelected ? Colors.white : Colors.white70,
                              fontSize: 11,
                            ),
                          );
                        }).toList(),
                      ),
                      const Divider(color: Colors.white24),
                      Row(
                        children: const [
                          Icon(Icons.compare_arrows, size: 16, color: Colors.white70),
                          SizedBox(width: 4),
                          Text(
                            'FLIP',
                            style: TextStyle(fontSize: 12, fontWeight: FontWeight.w500, color: Colors.white70),
                          ),
                        ],
                      ),
                      const SizedBox(height: 6),
                      Wrap(
                        spacing: 6,
                        runSpacing: 4,
                        children: VideoFlipMethod.values.map((method) {
                          final isSelected = currentFlipMethod == method;
                          final label = method.name.toUpperCase();
                          return ChoiceChip(
                            label: Text(label),
                            labelPadding: const EdgeInsets.symmetric(horizontal: 8),
                            materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
                            visualDensity: VisualDensity.compact,
                            selected: isSelected,
                            onSelected: (selected) {
                              if (selected) {
                                setModalState(() => currentFlipMethod = method);
                                setState(() => currentFlipMethod = method);
                                player.setVideoFlip(method);
                              }
                            },
                            selectedColor: Colors.cyanAccent.withOpacity(0.3),
                            backgroundColor: Colors.grey[800],
                            labelStyle: TextStyle(
                              color: isSelected ? Colors.white : Colors.white70,
                              fontSize: 11,
                            ),
                          );
                        }).toList(),
                      ),
                      const SizedBox(height: 12),
                    ],
                  ),
                ),
              );
            });
          },
        );
      },
    );
  }

  double _getVolumeForId(String id) {
    switch (id) {
      case 'bgm':
        return bgmVolume;
      case 'vocal':
        return vocalVolume;
      case 'guide':
        return guideVolume;
      case 'metronome':
        return metronomeVolume;
      default:
        return 0.0;
    }
  }

  Widget _buildVideoPlaceholder() {
    return Container(
      color: Colors.black,
      child: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.videocam_off, size: 48, color: Colors.white24),
            SizedBox(height: 8),
            Text("No Video Loaded", style: TextStyle(color: Colors.white54)),
          ],
        ),
      ),
    );
  }

  Widget _buildTrackControl(String label, String id, IconData icon, double volume,
      {bool isToggle = false, bool isEnabled = true}) {
    bool isActive = isToggle ? isEnabled : volume > 0;

    return CompositedTransformTarget(
      link: _layerLinks[id]!,
      child: GestureDetector(
        onTap: () {
          if (isToggle) {
            _toggleTrack(id);
          } else {
            _toggleMute(id);
          }
        },
        onLongPress: () {
          setState(() {
            showVolumeBars.updateAll((key, value) => false);
            showVolumeBars[id] = true;
          });

          Future.delayed(const Duration(seconds: 4), () {
            if (mounted && showVolumeBars[id] == true) {
              setState(() => showVolumeBars[id] = false);
            }
          });
        },
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: isActive ? Colors.cyanAccent.withValues(alpha: 0.2) : Colors.transparent,
                shape: BoxShape.circle,
                border: Border.all(
                  color: isActive ? Colors.cyanAccent : Colors.grey,
                  width: 2,
                ),
              ),
              child: Icon(icon, color: isActive ? Colors.cyanAccent : Colors.grey, size: 24),
            ),
            const SizedBox(height: 4),
            Text(label, style: TextStyle(color: isActive ? Colors.white : Colors.grey, fontSize: 10)),
          ],
        ),
      ),
    );
  }

  void _discardPage() {
    setState(() {
      isDiscarding = true;
    });
    if (player.isPlaying()) {
      player.pause();
    }
    player.dispose();
    Future.delayed(Duration(milliseconds: 500), () {
      player = UnifiedAVPlayerController();
      _initializePlayer();
      setState(() {
        hasAudioLoaded = false;
        hasVideoLoaded = false;
        isDiscarding = false;
        progress = 0.0;
        isPlaying = false;
        state = JuceMixPlayerState.IDLE;
        lastMixerComposeModel = null;
        currentVideoPath = null;
      });
    });
  }

  void _changeVideo() {
    player.pause();
    showModalBottomSheet(
      context: context,
      backgroundColor: const Color(0xFF1E1E1E),
      builder: (context) => Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          ListTile(
            leading: const Icon(Icons.video_library, color: Colors.cyanAccent),
            title: const Text('Load Sample Video', style: TextStyle(color: Colors.white)),
            onTap: () {
              Navigator.pop(context);
              _loadSampleVideo();
            },
          ),
          ListTile(
            leading: const Icon(Icons.folder_open, color: Colors.cyanAccent),
            title: const Text('Load from Gallery', style: TextStyle(color: Colors.white)),
            onTap: () {
              Navigator.pop(context);
              _loadVideoFromGallery();
            },
          ),
        ],
      ),
    );
  }

  void _toggleOverlay() {
    setState(() {
      isOverlayVisible = !isOverlayVisible;
    });
    if (isOverlayVisible) {
      _resetOverlayTimer();
    } else {
      _overlayTimer?.cancel();
    }
  }

  void _resetOverlayTimer() {
    _overlayTimer?.cancel();
    _overlayTimer = Timer(const Duration(seconds: 3), () {
      if (mounted && isPlaying) {
        setState(() => isOverlayVisible = false);
      }
    });
  }

  void _toggleTrack(String id) {
    if (id == 'metronome') {
      setState(() => metronomeEnabled = !metronomeEnabled);
      _updateAudioMix();
    } else if (id == 'guide') {
      setState(() => guideEnabled = !guideEnabled);
    }
  }

  void _toggleMute(String id) {
    if (id == 'bgm') {
      setState(() {
        bgmVolume = bgmVolume > 0 ? 0 : 0.7;
      });
    } else if (id == 'vocal') {
      setState(() {
        vocalVolume = vocalVolume > 0 ? 0 : 1.0;
      });
    } else if (id == 'guide') {
      setState(() {
        guideVolume = guideVolume > 0 ? 0 : 1.0;
      });
    } else if (id == 'metronome') {
      setState(() {
        metronomeVolume = metronomeVolume > 0 ? 0 : 1.0;
      });
    }
    _updateAudioMix();
  }

  void _updateTrackVolume(String id, double value, {bool shouldUpdateMix = true}) {
    setState(() {
      if (id == 'bgm') bgmVolume = value;
      if (id == 'vocal') vocalVolume = value;
      if (id == 'guide') guideVolume = value;
      if (id == 'metronome') metronomeVolume = value;
    });
    if (shouldUpdateMix) {
      _updateAudioMix();
    }
  }

  Widget _buildLatencyAdjustmentWidget() {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: Colors.black.withValues(alpha: 0.6),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: Colors.cyanAccent.withValues(alpha: 0.3), width: 1),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          InkWell(
            onTap: () {
              setState(() {
                latencyAdjustmentMs = (latencyAdjustmentMs - latencyStepMs).clamp(-maxLatencyMs, maxLatencyMs);
              });
              _updateInternalTrimValues();
            },
            child: Container(
              padding: const EdgeInsets.all(4),
              child: Icon(Icons.remove, color: Colors.white, size: 20),
            ),
          ),
          const SizedBox(width: 8),
          Text(
            '${latencyAdjustmentMs}ms',
            style: const TextStyle(
              color: Colors.white,
              fontSize: 14,
              fontWeight: FontWeight.w500,
            ),
          ),
          const SizedBox(width: 8),
          InkWell(
            onTap: () {
              setState(() {
                latencyAdjustmentMs = (latencyAdjustmentMs + latencyStepMs).clamp(-maxLatencyMs, maxLatencyMs);
              });
              _updateInternalTrimValues();
            },
            child: Container(
              padding: const EdgeInsets.all(4),
              child: Icon(Icons.add, color: Colors.white, size: 20),
            ),
          ),
        ],
      ),
    );
  }

  int _calculateCurrentUserPosition() {
    // progress is 0.0-1.0 based on master duration
    // Map to user trim window
    final trimDurationMs = userTrimEndMs - userTrimStartMs;
    final currentOffsetMs = progress * trimDurationMs;
    return userTrimStartMs + currentOffsetMs.toInt();
  }

  void _debugPrintTrimState(String context) {
    print('=== TRIM STATE: $context ===');
    print('USER VALUES:');
    print('  Video Duration: ${userVideoDurationMs}ms');
    print('  Trim: [${userTrimStartMs}ms - ${userTrimEndMs}ms]');
    print('  Trim Duration: ${userTrimEndMs - userTrimStartMs}ms');
    print('INTERNAL VALUES:');
    print('  Video Duration: ${internalVideoDurationMs}ms (includes ${maxLatencyMs}ms padding)');
    print('  Trim: [${internalTrimStartMs}ms - ${internalTrimEndMs}ms]');
    print('  Trim Duration: ${internalTrimEndMs - internalTrimStartMs}ms');
    print('LATENCY:');
    print('  Adjustment: ${latencyAdjustmentMs}ms');
    print('  Max: ±${maxLatencyMs}ms');
    print('===============================');
  }

  Widget _buildVideoView() {
    return UnifiedVideoView(
      controller: player,
      onViewReady: () {
        print('UnifiedVideoView: Ready');
      },
    );
  }

  Future<void> _exportVideo() async {
    setState(() => isExporting = true);
    try {
      final dirPath = await AssetHelper.getApplicationDocumentsDirectoryPath();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      final outputPath = '$dirPath/exported_video_$timestamp.mp4';
      await player.exportVideo(outputPath);
      _showSnack('Exported to: $outputPath', isSuccess: true);
    } catch (e) {
      _showSnack('Export failed: $e', isError: true);
    } finally {
      setState(() => isExporting = false);
    }
  }
}
