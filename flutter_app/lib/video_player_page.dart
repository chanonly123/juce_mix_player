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
  GstPlayerController player = GstPlayerController();
  double _seekPosition = 0.0;
  double _currentPosition = 0.0;
  double _duration = 0.0;
  bool _isViewReady = false;
  bool _isPlaying = false;
  bool _isMuted = true;
  String _playerState = "IDLE";
  double _progressUpdateInterval = 0.05;
  bool _hasVideoLoaded = false;
  bool _showPreview = false;

  // Video processing state
  String _currentRotation = "0";
  VisualEffectType _currentEffect = VisualEffectType.none;
  bool _isExporting = false;

  @override
  void initState() {
    super.initState();

    // Set up progress handler for real-time updates
    player.setProgressHandler((progress) {
      if (mounted) {
        setState(() {
          _seekPosition = progress;
          _currentPosition = progress * player.getDuration();
          _duration = player.getDuration();
        });
      }
    });

    // Set up state update handler
    player.setStateUpdateHandler((state) {
      if (mounted) {
        setState(() {
          _playerState = state;
          _isPlaying = player.isPlaying();

          // When video is ready, show preview and get duration
          if (state == "READY" && !_showPreview) {
            _showPreview = true;
            _hasVideoLoaded = true;
            _duration = player.getDuration();

            // Seek to first frame for preview (position 0)
            Future.delayed(const Duration(milliseconds: 100), () {
              player.seek(0.0);
            });
          }

          // Reset preview when stopped or error
          if (state == "STOPPED" || state == "ERROR" || state == "IDLE") {
            _showPreview = false;
            _hasVideoLoaded = false;
            _duration = 0.0;
            _currentPosition = 0.0;
            _seekPosition = 0.0;
          }
        });
      }
    });

    // Set up error handler
    player.setErrorHandler((error) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Error: $error'),
            backgroundColor: Colors.red,
          ),
        );
      }
    });
  }

  Future<void> pickVideoFromGallery() async {
    final ImagePicker picker = ImagePicker();
    final XFile? video = await picker.pickVideo(source: ImageSource.gallery);

    if (video != null) {
      setState(() {
        _hasVideoLoaded = false;
        _showPreview = false;
      });

      player.setVideoPath(video.path);

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Video loaded: ${video.name}'),
          backgroundColor: Colors.green,
        ),
      );
    }
  }

  void toggleMute() {
    player.toggleMute();
    setState(() {
      _isMuted = player.isMuted();
    });
  }

  void rotateVideo(String degrees) {
    try {
      player.setRotation(degrees);
      setState(() {
        _currentRotation = degrees;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Video rotated to $degrees degrees'),
          backgroundColor: Colors.green,
        ),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error rotating video: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  void applyVisualEffect(VisualEffectType effect) {
    try {
      player.setVisualEffect(effect);
      setState(() {
        _currentEffect = effect;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Applied effect: ${effect.name.toUpperCase()}'),
          backgroundColor: Colors.green,
        ),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error applying effect: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  Future<void> exportVideo() async {
    if (!_hasVideoLoaded) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('No video loaded to export'),
          backgroundColor: Colors.red,
        ),
      );
      return;
    }

    setState(() {
      _isExporting = true;
    });

    try {
      final directory = await getApplicationDocumentsDirectory();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      final outputPath = '${directory.path}/exported_video_$timestamp.mp4';

      await player.exportVideo(outputPath);

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Video exported successfully to: $outputPath'),
          backgroundColor: Colors.green,
          duration: const Duration(seconds: 3),
        ),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Export failed: $e'),
          backgroundColor: Colors.red,
        ),
      );
    } finally {
      setState(() {
        _isExporting = false;
      });
    }
  }

  Color _getStateColor(String state) {
    switch (state.toUpperCase()) {
      case 'PLAYING':
        return Colors.green;
      case 'PAUSED':
        return Colors.orange;
      case 'STOPPED':
        return Colors.red;
      case 'ERROR':
        return Colors.red;
      case 'READY':
        return Colors.blue;
      default:
        return Colors.grey;
    }
  }

  IconData _getStateIcon(String state) {
    switch (state.toUpperCase()) {
      case 'PLAYING':
        return Icons.play_arrow;
      case 'PAUSED':
        return Icons.pause;
      case 'STOPPED':
        return Icons.stop;
      case 'ERROR':
        return Icons.error;
      case 'READY':
        return Icons.check_circle;
      default:
        return Icons.info;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Video Player'),
        actions: [
          IconButton(
            icon: Icon(
              _isMuted ? Icons.volume_off : Icons.volume_up,
              color: _isMuted ? Colors.red : Colors.blue,
            ),
            onPressed: toggleMute,
            tooltip: _isMuted ? 'Unmute' : 'Mute',
          ),
        ],
      ),
      body: Column(
        children: [
          // Player State Display
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(12),
            margin: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: _getStateColor(_playerState).withOpacity(0.1),
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: _getStateColor(_playerState)),
            ),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(_getStateIcon(_playerState), color: _getStateColor(_playerState)),
                const SizedBox(width: 8),
                Text(
                  'State: $_playerState',
                  style: TextStyle(
                    color: _getStateColor(_playerState),
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
          ),
          Expanded(
            child: Center(
              child: AspectRatio(
                aspectRatio: 9 / 16,
                child: Stack(
                  children: [
                    GstVideoView(
                      controller: player,
                      onViewReady: () {
                        setState(() {
                          _isViewReady = true;
                        });
                      },
                    ),

                    // Preview overlay when video is loaded but not playing
                    if (_hasVideoLoaded && !_isPlaying && _showPreview)
                      Container(
                        decoration: BoxDecoration(
                          color: Colors.black.withOpacity(0.3),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Container(
                                padding: const EdgeInsets.all(20),
                                decoration: const BoxDecoration(
                                  color: Colors.black54,
                                  shape: BoxShape.circle,
                                ),
                                child: const Icon(
                                  Icons.play_arrow,
                                  color: Colors.white,
                                  size: 60,
                                ),
                              ),
                              const SizedBox(height: 16),
                              Container(
                                padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                                decoration: BoxDecoration(
                                  color: Colors.black54,
                                  borderRadius: BorderRadius.circular(20),
                                ),
                                child: Text(
                                  'Video Ready',
                                  textAlign: TextAlign.center,
                                  style: const TextStyle(
                                    color: Colors.white,
                                    fontSize: 14,
                                    fontWeight: FontWeight.w500,
                                  ),
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                  ],
                ),
              ),
            ),
          ),

          const SizedBox(height: 16),

          // Time Display
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 20),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  TimeUtils.formatDuration(_currentPosition),
                  style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w500),
                ),
                Text(
                  TimeUtils.formatDuration(_duration),
                  style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w500),
                ),
              ],
            ),
          ),

          // Progress Slider
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 20),
            child: Column(
              children: [
                Slider(
                  value: _seekPosition,
                  min: 0.0,
                  max: 1.0,
                  onChanged: (value) {
                    setState(() {
                      _seekPosition = value;
                    });
                  },
                  onChangeEnd: (value) {
                    player.seek(value);
                  },
                ),
                Text(
                  '${(_seekPosition * 100).toStringAsFixed(1)}%',
                  style: const TextStyle(fontSize: 12, color: Colors.grey),
                ),
              ],
            ),
          ),

          const SizedBox(height: 16),

          // Control Buttons
          Wrap(
            alignment: WrapAlignment.center,
            spacing: 10,
            runSpacing: 10,
            children: [
              ElevatedButton.icon(
                onPressed: pickVideoFromGallery,
                icon: const Icon(Icons.photo_library),
                label: const Text('Pick Video'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.blue,
                  foregroundColor: Colors.white,
                ),
              ),
              ElevatedButton(
                onPressed: _isViewReady
                    ? () async {
                        final pathL = await AssetHelper.extractAsset('assets/media/Fate_of_Ophelia.mp4');
                        print('Loading video file: $pathL');
                        player.setVideoPath(pathL);
                      }
                    : null,
                child: const Text('Load Sample'),
              ),
            ],
          ),

          const SizedBox(height: 16),

          // Playback Controls
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              ElevatedButton.icon(
                onPressed: _isViewReady && (_hasVideoLoaded || _isPlaying)
                    ? () {
                        player.togglePlayPause();
                      }
                    : null,
                icon: Icon(_isPlaying ? Icons.pause : Icons.play_arrow),
                label: Text(_isPlaying ? 'Pause' : 'Play'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: _isPlaying ? Colors.orange : Colors.green,
                  foregroundColor: Colors.white,
                ),
              ),
              const SizedBox(width: 20),
              ElevatedButton.icon(
                onPressed: _isViewReady
                    ? () {
                        player.stop();
                      }
                    : null,
                icon: const Icon(Icons.stop),
                label: const Text('Stop'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red,
                  foregroundColor: Colors.white,
                ),
              ),
            ],
          ),

          const SizedBox(height: 20),

          // Video Processing Controls
          if (_hasVideoLoaded) ...[
            Container(
              padding: const EdgeInsets.all(16),
              margin: const EdgeInsets.symmetric(horizontal: 16),
              decoration: BoxDecoration(
                color: Colors.blue.withValues(alpha: 0.1),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: Colors.blue.withValues(alpha: 0.3)),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    'Video Processing',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 16),

                  // Rotation Controls
                  const Text(
                    'Rotation:',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    children: ['0', '90', '180', '270'].map((degrees) {
                      return ChoiceChip(
                        label: Text('${degrees}°'),
                        selected: _currentRotation == degrees,
                        onSelected: (selected) {
                          if (selected) rotateVideo(degrees);
                        },
                        selectedColor: Colors.blue.withValues(alpha: 0.3),
                      );
                    }).toList(),
                  ),

                  const SizedBox(height: 16),

                  // Visual Effects Controls
                  const Text(
                    'Visual Effects:',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: VisualEffectType.values.map((effect) {
                      return ChoiceChip(
                        label: Text(effect.name.toUpperCase()),
                        selected: _currentEffect == effect,
                        onSelected: (selected) {
                          if (selected) applyVisualEffect(effect);
                        },
                        selectedColor: Colors.purple.withValues(alpha: 0.3),
                      );
                    }).toList(),
                  ),

                  const SizedBox(height: 16),

                  // Export Button
                  Center(
                    child: ElevatedButton.icon(
                      onPressed: _isExporting ? null : exportVideo,
                      icon: _isExporting
                          ? const SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Icon(Icons.download),
                      label: Text(_isExporting ? 'Exporting...' : 'Export Video'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.green,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
                      ),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 20),
          ],

          // Progress Update Interval Configuration
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Progress Update Interval: ${(_progressUpdateInterval * 1000).toInt()}ms',
                  style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w500),
                ),
                Slider(
                  value: _progressUpdateInterval,
                  min: 0.01,
                  max: 0.5,
                  divisions: 49,
                  onChanged: (value) {
                    setState(() {
                      _progressUpdateInterval = value;
                    });
                    // Note: This would need to be implemented in the native side
                    // to actually change the update interval
                  },
                ),
              ],
            ),
          ),

          const SizedBox(height: 16),
        ],
      ),
    );
  }

  @override
  void dispose() {
    player.dispose();
    super.dispose();
  }
}
