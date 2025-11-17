import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:juce_mix_player/gst_player.dart';
import 'package:juce_mix_player/gst_video_view.dart';

class VideoPlayerPage extends StatefulWidget {
  const VideoPlayerPage({super.key});

  @override
  VideoPlayerState createState() => VideoPlayerState();
}

class VideoPlayerState extends State<VideoPlayerPage> {
  GstPlayer player = GstPlayer();
  double _seekPosition = 0.0;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Video Player'),
      ),
      body: Column(
        children: [
          Expanded(
            child: Center(
              child: AspectRatio(
                aspectRatio: 9 / 16,
                child: GstVideoView(),
              ),
            ),
          ),
          const SizedBox(height: 16),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 20),
            child: Column(
              children: [
                Row(
                  children: [
                    Text('${(_seekPosition * 100).toStringAsFixed(0)}%'),
                    Expanded(
                      child: Slider(
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
                    ),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          Row(mainAxisAlignment: MainAxisAlignment.center, children: [
            ElevatedButton(
              onPressed: () async {
                // player.setUrl(
                //     'https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4');

                final pathL = await AssetHelper.extractAsset(
                    'assets/media/BigBuckBunny.mp4');
                print('Loading audio file: $pathL');
                bool success = player.setUrl(pathL);
                if (success) {
                  print('URL set successfully, starting playback');
                  player.play();
                } else {
                  print('Failed to set URL');
                }
              },
              child: const Text('Play'),
            ),
            const SizedBox(width: 20),
            ElevatedButton(
              onPressed: () {
                print('Pausing playback');
                player.pause();
              },
              child: const Text('Pause'),
            ),
            const SizedBox(width: 20),
            ElevatedButton(
              onPressed: () {
                print('Stopping playback');
                player.stop();
              },
              child: const Text('Stop'),
            ),
          ]),
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
