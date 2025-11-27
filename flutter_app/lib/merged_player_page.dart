import 'dart:async';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/utils.dart';
import 'package:image_picker/image_picker.dart';
import 'package:juce_mix_player/gst_video_player.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:juce_mix_player/unified_av_player.dart';
import 'package:juce_mix_player/unified_video_view.dart';

class MergedPlayerPage extends StatefulWidget {
  const MergedPlayerPage({super.key});

  @override
  MergedPlayerPageState createState() => MergedPlayerPageState();
}

class MergedPlayerPageState extends State<MergedPlayerPage> {
  final player = UnifiedAVPlayerController();

  // Playback state
  double progress = 0.0;
  bool isSliderEditing = false;
  bool isPlaying = false;
  JuceMixPlayerState state = JuceMixPlayerState.IDLE;
  bool hasVideoLoaded = false;

  // Audio settings
  double bgmVolume = 0.7;
  double vocalVolume = 1.0;
  bool guideEnabled = false;
  bool metronomeEnabled = false;
  MixerComposeModel? lastMixerComposeModel;

  // Video state
  bool isVideoViewReady = false;
  int currentRotation = 0;
  VisualEffectType currentEffect = VisualEffectType.none;
  bool isExporting = false;

  // UI state
  bool isAudioPanelExpanded = true;
  bool isVideoPanelExpanded = false;
  bool isVideoLoading = false;

  // New UI State
  bool hasAudioLoaded = false;
  bool isOverlayVisible = true;
  Timer? _overlayTimer;
  Map<String, bool> showVolumeBars = {
    'bgm': false,
    'vocal': false,
    'guide': false,
    'metronome': false,
  };

  // Separate volumes
  double guideVolume = 1.0;
  double metronomeVolume = 1.0;

  // Overlay positioning
  final Map<String, LayerLink> _layerLinks = {
    'bgm': LayerLink(),
    'vocal': LayerLink(),
    'guide': LayerLink(),
    'metronome': LayerLink(),
  };

  bool isDiscarding = false;

  @override
  void initState() {
    super.initState();
    _initializePlayer();
  }

  void _initializePlayer() {
    // Playback callbacks
    player.setProgressHandler((progress) {
      if (!isSliderEditing && mounted) {
        setState(() => this.progress = progress);
      }
    });

    player.setStateUpdateHandler((newState) {
      if (mounted) {
        setState(() {
          state = newState;
          isPlaying = newState == JuceMixPlayerState.PLAYING;
        });
      }
    });

    player.setErrorHandler((error) {
      if (mounted) {
        _showSnack('Error: $error', isError: true);
      }
    });

    // Set default settings
    player.setAudioSettings(MixerSettings(
      progressUpdateInterval: 0.05,
    ));
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

  // Audio loading methods
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
      isVideoLoading = true;
      hasVideoLoaded = false;
    });
    try {
      final path = await AssetHelper.extractAsset('assets/media/Fate_of_Ophelia_muted.mp4');
      player.setVideoPath(path);
      setState(() {
        hasVideoLoaded = true;
        isVideoLoading = false;
      });
      _showSnack('Sample video loaded', isSuccess: true);
    } catch (e) {
      setState(() => isVideoLoading = false);
      _showSnack('Error loading video: $e', isError: true);
    }
  }

  Future<void> _loadVideoFromGallery() async {
    final ImagePicker picker = ImagePicker();
    setState(() {
      isVideoLoading = true;
      hasVideoLoaded = false;
    });
    final XFile? video = await picker.pickVideo(source: ImageSource.gallery);
    if (video != null) {
      try {
        player.setVideoPath(video.path);
        setState(() {
          hasVideoLoaded = true;
          isVideoLoading = false;
        });
        _showSnack('Video loaded: ${video.name}', isSuccess: true);
      } catch (e) {
        setState(() => isVideoLoading = false);
        _showSnack('Error loading video: $e', isError: true);
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
          // Assuming other tracks are guide or vocal if not explicitly handled,
          // but for now let's stick to what we know.
          // If there was a 'guide' track in the model, we'd handle it here.
          // Since the sample model only has bgm and metronome, we'll just return as is or update if id matches.
          return track.copyWith(volume: bgmVolume, enabled: metronomeEnabled);
        }
      }).toList(),
    );
    player.setAudioData(lastMixerComposeModel!);
  }

  @override
  void dispose() {
    UnifiedAVPlayerController.destroyInstance();
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
    return Stack(
      children: [
        Column(
          children: [
            // Video Area (Top)
            Expanded(
              child: Stack(
                fit: StackFit.expand,
                children: [
                  // Video View
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

                  // Overlay Controls (Tap to show/hide)
                  Positioned.fill(
                    child: GestureDetector(
                      behavior: HitTestBehavior.translucent,
                      onTap: _toggleOverlay,
                      child: AnimatedOpacity(
                        opacity: isOverlayVisible ? 1.0 : 0.0,
                        duration: const Duration(milliseconds: 300),
                        child: Container(
                          color: Colors.black26,
                          child: Stack(
                            children: [
                              // Center Play/Pause
                              Center(
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

                              // Bottom Seeker
                              Positioned(
                                bottom: 16,
                                left: 16,
                                right: 16,
                                child: Column(
                                  mainAxisSize: MainAxisSize.min,
                                  children: [
                                    Row(
                                      children: [
                                        Text(
                                          TimeUtils.formatDuration(progress * player.getDuration()),
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
                                          TimeUtils.formatDuration(player.getDuration()),
                                          style: const TextStyle(color: Colors.white, fontSize: 12),
                                        ),
                                      ],
                                    ),
                                  ],
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),

                  // Top Bar (Always Visible)
                  Positioned(
                    top: MediaQuery.of(context).padding.top + 16,
                    left: 16,
                    right: 16,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        // Discard Button
                        IconButton(
                          icon: const Icon(Icons.remove_circle, color: Colors.red),
                          onPressed: _discardPage,
                          tooltip: 'Discard & Reload',
                        ),
                        // Export Button
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

            // Bottom Controls Area
            Container(
              color: const Color(0xFF1E1E1E),
              padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 8),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  // Track Controls Row
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
                  // Bottom Action Buttons
                  Row(
                    children: [
                      IconButton(
                        onPressed: () {
                          setState(() {
                            hasVideoLoaded = false;
                            isVideoViewReady = false;
                          });
                        },
                        icon: const Icon(Icons.videocam_off_outlined, color: Colors.redAccent),
                        tooltip: 'Remove Video',
                      ),
                      const SizedBox(width: 8),
                      // Video Settings (Expanded)
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
                        icon: Icon(hasVideoLoaded ? Icons.video_library : Icons.add_circle_outline,
                            color: Colors.cyanAccent),
                        tooltip: hasVideoLoaded ? 'Replace Video' : 'Add Video',
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),

        // Volume Overlay Layer
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
                    offset: const Offset(-15, -150), // Adjust to position above
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
      ],
    );
  }

  void _showVideoSettings() {
    showModalBottomSheet(
      context: context,
      barrierColor: Colors.transparent,
      backgroundColor: const Color(0xFF1E1E1E),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(16)),
      ),
      builder: (context) => StatefulBuilder(builder: (context, setModalState) {
        return Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text('Video Settings',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Colors.white)),
              const SizedBox(height: 16),

              // Rotation controls
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text('Rotation', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.white70)),
                  Text(
                    '${currentRotation.clamp(0, 359)}°',
                    style: const TextStyle(color: Colors.white70, fontSize: 12),
                  ),
                ],
              ),
              const SizedBox(height: 8),
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

              const SizedBox(height: 16),
              const Divider(color: Colors.white24),
              const SizedBox(height: 8),

              // Visual effects
              const Text('Visual Effects', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.white70)),
              const SizedBox(height: 8),
              Wrap(
                spacing: 8,
                children: VisualEffectType.values.map((effect) {
                  bool isSelected = currentEffect == effect;
                  return ChoiceChip(
                    label: Text(effect.name.toUpperCase()),
                    selected: isSelected,
                    onSelected: (selected) {
                      if (selected) {
                        setModalState(() => currentEffect = effect);
                        setState(() => currentEffect = effect);
                        player.setVideoVisualEffect(effect.index);
                      }
                    },
                    selectedColor: Colors.cyanAccent.withOpacity(0.3),
                    backgroundColor: Colors.grey[800],
                    labelStyle: TextStyle(color: isSelected ? Colors.white : Colors.white70),
                  );
                }).toList(),
              ),
              const SizedBox(height: 24),
            ],
          ),
        );
      }),
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
            // Toggle mute for volume tracks
            _toggleMute(id);
          }
        },
        onLongPress: () {
          setState(() {
            // Hide others
            showVolumeBars.updateAll((key, value) => false);
            showVolumeBars[id] = true;
          });

          // Auto hide after 3 seconds of no interaction (simple timer for now)
          // Ideally we reset this timer on slider interaction, but for simplicity:
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

  // Logic Helpers

  void _discardPage() async {
    setState(() => isDiscarding = true);
    // Simulate buffer
    if (mounted) {
      // Reset everything
      player.pause();
      setState(() {
        //   isDiscarding = false;
        //   hasAudioLoaded = false;
        hasVideoLoaded = false;
        isVideoViewReady = false;
        //   progress = 0.0;
        //   isPlaying = false;
        //   state = JuceMixPlayerState.IDLE;
        //   // Reset other settings if needed
      });
      UnifiedAVPlayerController.destroyInstance();
    }
    await Future.delayed(const Duration(seconds: 3));
    setState(() {
      isDiscarding = false;
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
      // Update mix if guide logic exists
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

  // Use UnifiedVideoView for direct UnifiedAVPlayer integration
  Widget _buildVideoView() {
    return UnifiedVideoView(
      controller: player,
      onViewReady: () {
        setState(() => isVideoViewReady = true);
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
