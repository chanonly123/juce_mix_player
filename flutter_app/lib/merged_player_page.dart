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
  String currentRotation = "0";
  VisualEffectType currentEffect = VisualEffectType.none;
  bool isExporting = false;

  // UI state
  bool isAudioPanelExpanded = true;
  bool isVideoPanelExpanded = false;

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

          if (newState == JuceMixPlayerState.READY) {
            _showSnack('Player ready!', isSuccess: true);
          }
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
        _showSnack('Audio loaded: $fileName', isSuccess: true);
      } else {
        _showSnack('No file selected');
      }
    } catch (e) {
      _showSnack('Error loading audio: $e', isError: true);
    }
  }

  Future<void> _loadSampleVideo() async {
    final path = await AssetHelper.extractAsset('assets/media/Fate_of_Ophelia_muted.mp4');
    player.setVideoPath(path);
    setState(() => hasVideoLoaded = true);
    _showSnack('Sample video loaded', isSuccess: true);
  }

  Future<void> _loadVideoFromGallery() async {
    final ImagePicker picker = ImagePicker();
    final XFile? video = await picker.pickVideo(source: ImageSource.gallery);
    if (video != null) {
      player.setVideoPath(video.path);
      setState(() => hasVideoLoaded = true);
      _showSnack('Video loaded: ${video.name}', isSuccess: true);
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
        } else {
          return track.copyWith(volume: bgmVolume, enabled: metronomeEnabled);
        }
      }).toList(),
    );
    player.setAudioData(lastMixerComposeModel!);
    _showSnack('Audio mix updated', isSuccess: true);
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
        appBar: AppBar(
          title: const Text('Unified A/V Player'),
          backgroundColor: const Color(0xFF1E1E1E),
          elevation: 0,
        ),
        body: Column(
          children: [
            // Video viewport (if video loaded)
            if (hasVideoLoaded)
              Expanded(
                flex: 3,
                child: _buildVideoViewport(),
              )
            else
              Expanded(
                flex: 3,
                child: _buildPlaceholderViewport(),
              ),

            // Unified playback controls
            _buildUnifiedControls(),

            // Scrollable panels area
            Expanded(
              flex: 2,
              child: SingleChildScrollView(
                child: Column(
                  children: [
                    _buildAudioPanel(),
                    if (hasVideoLoaded) _buildVideoPanel(),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildVideoViewport() {
    return Container(
      color: Colors.black,
      child: Stack(
        fit: StackFit.expand,
        children: [
          // Video view - match video_player_page.dart structure
          Center(
            child: Container(
              color: Colors.grey[900],
              child: _buildVideoView(),
            ),
          ),

          // Overlay controls
          Positioned(
            top: 16,
            right: 16,
            child: Container(
              decoration: BoxDecoration(
                color: Colors.black54,
                shape: BoxShape.circle,
              ),
              child: IconButton(
                icon: const Icon(Icons.close, color: Colors.white),
                onPressed: () {
                  setState(() {
                    hasVideoLoaded = false;
                    isVideoViewReady = false;
                  });
                  _showSnack('Video removed');
                },
              ),
            ),
          ),

          // Video info overlay (bottom left)
          Positioned(
            bottom: 16,
            left: 16,
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
              decoration: BoxDecoration(
                color: Colors.black54,
                borderRadius: BorderRadius.circular(20),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const Icon(Icons.videocam, size: 16, color: Colors.purpleAccent),
                  const SizedBox(width: 6),
                  Text(
                    'Video Active',
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 12,
                      fontWeight: FontWeight.w500,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
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

  Widget _buildPlaceholderViewport() {
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
              state == JuceMixPlayerState.IDLE ? 'Load audio to begin' : 'Playing audio',
              style: TextStyle(color: Colors.white54),
            ),
            const SizedBox(height: 24),
            if (!hasVideoLoaded)
              TextButton.icon(
                onPressed: _loadVideoFromGallery,
                icon: const Icon(Icons.video_library),
                label: const Text('Add Video (Optional)'),
                style: TextButton.styleFrom(
                  foregroundColor: Colors.cyanAccent,
                ),
              ),
            TextButton.icon(
              onPressed: _loadSampleVideo,
              icon: const Icon(Icons.video_library),
              label: const Text('Load Sample Video'),
              style: TextButton.styleFrom(
                foregroundColor: Colors.cyanAccent,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildUnifiedControls() {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.3),
            blurRadius: 10,
            offset: const Offset(0, -2),
          ),
        ],
      ),
      child: Column(
        children: [
          // Progress bar and time
          Row(
            children: [
              Text(
                TimeUtils.formatDuration(progress * player.getDuration()),
                style: const TextStyle(fontSize: 12, color: Colors.white70),
              ),
              Expanded(
                child: Slider(
                  value: progress,
                  onChanged: (value) {
                    setState(() => progress = value);
                  },
                  onChangeStart: (_) => isSliderEditing = true,
                  onChangeEnd: (value) {
                    isSliderEditing = false;
                    player.seek(value);
                  },
                ),
              ),
              Text(
                TimeUtils.formatDuration(player.getDuration()),
                style: const TextStyle(fontSize: 12, color: Colors.white70),
              ),
            ],
          ),

          const SizedBox(height: 8),

          // Main playback buttons
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              IconButton(
                iconSize: 32,
                icon: const Icon(Icons.skip_previous),
                onPressed: () => player.seek(0),
                color: Colors.white70,
              ),
              const SizedBox(width: 16),
              Container(
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  gradient: LinearGradient(
                    colors: [Colors.cyanAccent, Colors.blueAccent],
                  ),
                ),
                child: IconButton(
                  iconSize: 48,
                  icon: Icon(
                    isPlaying ? Icons.pause : Icons.play_arrow,
                    color: Colors.white,
                  ),
                  onPressed: state != JuceMixPlayerState.IDLE ? () => player.togglePlayPause() : null,
                ),
              ),
              const SizedBox(width: 16),
              IconButton(
                iconSize: 32,
                icon: const Icon(Icons.stop),
                onPressed: () => player.stop(),
                color: Colors.redAccent,
              ),
            ],
          ),

          const SizedBox(height: 8),

          // State indicator
          Text(
            state.name,
            style: TextStyle(
              fontSize: 11,
              color: _getStateColor(state),
              fontWeight: FontWeight.bold,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildAudioPanel() {
    return Card(
      margin: const EdgeInsets.all(8),
      color: const Color(0xFF2A2A2A),
      child: Column(
        children: [
          ListTile(
            leading: const Icon(Icons.audiotrack, color: Colors.cyanAccent),
            title: const Text('Audio Settings', style: TextStyle(fontWeight: FontWeight.bold)),
            trailing: IconButton(
              icon: Icon(
                isAudioPanelExpanded ? Icons.expand_less : Icons.expand_more,
              ),
              onPressed: () => setState(() => isAudioPanelExpanded = !isAudioPanelExpanded),
            ),
          ),
          if (isAudioPanelExpanded)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // Load audio buttons
                  Row(
                    children: [
                      Expanded(
                        child: ElevatedButton.icon(
                          onPressed: _loadSampleAudio,
                          icon: const Icon(Icons.music_note),
                          label: const Text('Load Sample'),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.blueAccent,
                          ),
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: ElevatedButton.icon(
                          onPressed: _loadAudioFromGallery,
                          icon: const Icon(Icons.folder),
                          label: const Text('From Gallery'),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.blueAccent,
                          ),
                        ),
                      ),
                    ],
                  ),

                  const SizedBox(height: 16),
                  const Divider(),

                  // BGM Volume
                  const Text('BGM Volume', style: TextStyle(fontWeight: FontWeight.bold)),
                  Row(
                    children: [
                      const Icon(Icons.volume_up, size: 20, color: Colors.orangeAccent),
                      Expanded(
                        child: Slider(
                          value: bgmVolume,
                          onChanged: (v) {
                            setState(() => bgmVolume = v);
                          },
                          onChangeEnd: (_) => _updateAudioMix(),
                        ),
                      ),
                      Text('${(bgmVolume * 100).toInt()}%', style: const TextStyle(fontSize: 12)),
                    ],
                  ),

                  // Vocal Volume
                  // const Text('Vocal Volume', style: TextStyle(fontWeight: FontWeight.bold)),
                  // Row(
                  //   children: [
                  //     const Icon(Icons.mic, size: 20, color: Colors.purpleAccent),
                  //     Expanded(
                  //       child: Slider(
                  //         value: vocalVolume,
                  //         onChanged: (v) {
                  //           setState(() => vocalVolume = v);
                  //         },
                  //         onChangeEnd: (_) => _updateAudioMix(),
                  //       ),
                  //     ),
                  //     Text('${(vocalVolume * 100).toInt()}%', style: const TextStyle(fontSize: 12)),
                  //   ],
                  // ),

                  const Divider(),

                  // Guide & Metronome toggles
                  // SwitchListTile(
                  //   title: const Text('Enable Guide'),
                  //   value: guideEnabled,
                  //   onChanged: (v) => setState(() => guideEnabled = v),
                  //   activeThumbColor: Colors.greenAccent,
                  // ),
                  SwitchListTile(
                    title: const Text('Enable Metronome'),
                    value: metronomeEnabled,
                    onChanged: (v) {
                      setState(() => metronomeEnabled = v);
                      _updateAudioMix();
                    },
                    activeThumbColor: Colors.greenAccent,
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildVideoPanel() {
    return Card(
      margin: const EdgeInsets.all(8),
      color: const Color(0xFF2A2A2A),
      child: Column(
        children: [
          ListTile(
            leading: const Icon(Icons.videocam, color: Colors.purpleAccent),
            title: const Text('Video Settings', style: TextStyle(fontWeight: FontWeight.bold)),
            trailing: IconButton(
              icon: Icon(
                isVideoPanelExpanded ? Icons.expand_less : Icons.expand_more,
              ),
              onPressed: () => setState(() => isVideoPanelExpanded = !isVideoPanelExpanded),
            ),
          ),
          if (isVideoPanelExpanded)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // Rotation controls
                  const Text('Rotation', style: TextStyle(fontWeight: FontWeight.bold)),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    children: ['0', '45', '90', '135', '180', '225', '270'].map((deg) {
                      bool isSelected = currentRotation == deg;
                      return ChoiceChip(
                        label: Text('$deg°'),
                        selected: isSelected,
                        onSelected: (selected) {
                          if (selected) {
                            setState(() => currentRotation = deg);
                            player.setVideoRotation(int.parse(deg));
                          }
                        },
                        selectedColor: Colors.purpleAccent.withValues(alpha: 0.3),
                      );
                    }).toList(),
                  ),

                  const SizedBox(height: 16),
                  const Divider(),

                  // Visual effects
                  const Text('Visual Effects', style: TextStyle(fontWeight: FontWeight.bold)),
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
                            setState(() => currentEffect = effect);
                            player.setVideoVisualEffect(effect.index);
                          }
                        },
                        selectedColor: Colors.cyanAccent.withValues(alpha: 0.3),
                      );
                    }).toList(),
                  ),

                  const SizedBox(height: 16),

                  // Export video button
                  SizedBox(
                    width: double.infinity,
                    child: ElevatedButton.icon(
                      onPressed: isExporting ? null : _exportVideo,
                      icon: isExporting
                          ? const SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Icon(Icons.download),
                      label: Text(isExporting ? 'EXPORTING...' : 'EXPORT VIDEO'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.green.shade700,
                      ),
                    ),
                  ),
                ],
              ),
            ),
        ],
      ),
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

  Color _getStateColor(JuceMixPlayerState state) {
    switch (state) {
      case JuceMixPlayerState.PLAYING:
        return Colors.greenAccent;
      case JuceMixPlayerState.PAUSED:
        return Colors.orangeAccent;
      case JuceMixPlayerState.STOPPED:
        return Colors.redAccent;
      case JuceMixPlayerState.ERROR:
        return Colors.red;
      case JuceMixPlayerState.READY:
        return Colors.cyanAccent;
      default:
        return Colors.grey;
    }
  }
}
