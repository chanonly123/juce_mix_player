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

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Video Player'),
      ),
      body: Column(
        children: [
          Expanded(child: Container()),
          LayoutBuilder(
            builder: (context, constraints) {
              final double maxWidth = constraints.maxWidth;
              final double width = maxWidth;
              final double height = width / 16 * 9;
              return Center(
                child: SizedBox(
                  width: width,
                  height: height,
                  child: GstVideoView(),
                ),
              );
            },
          ),
          Row(mainAxisAlignment: MainAxisAlignment.center, children: [
            ElevatedButton(
              onPressed: () async {
                // player.setUrl(
                //     'https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4');

                final pathL = await AssetHelper.extractAsset(
                    'assets/media/music_small.wav');
                player.setUrl(pathL);
              },
              child: const Text('Play'),
            ),
            SizedBox(width: 20),
            ElevatedButton(
              onPressed: () {
                player.stop();
              },
              child: const Text('Stop'),
            ),
          ]),
          Expanded(child: Container()),
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
