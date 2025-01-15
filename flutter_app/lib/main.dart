import 'dart:developer';

import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/juce_lib/juce_mix_models.dart';
import 'package:flutter_app/juce_lib/juce_mix_player.dart';

import 'juce_lib/juce_lib.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  JuceLib().juce_enableLogs(1);
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: PlayerPage(),
    );
  }
}

class PlayerPage extends StatefulWidget {
  const PlayerPage({super.key});

  @override
  PlayerPageState createState() => PlayerPageState();
}

class PlayerPageState extends State<PlayerPage> {
  final player = JuceMixPlayer();
  double progress = 0.0;
  bool isSliderEditing = false;
  bool isPlaying = false;

  JuceMixPlayerState state = JuceMixPlayerState.IDLE;

  @override
  void initState() {
    super.initState();

    player.setStateUpdateHandler((state) {
      setState(() => this.state = state);
      switch (state) {
        case JuceMixPlayerState.PAUSED:
          setState(() => isPlaying = false);
        case JuceMixPlayerState.PLAYING:
          setState(() => isPlaying = true);
        case JuceMixPlayerState.IDLE:
        // TODO: Handle this case.
        case JuceMixPlayerState.READY:
        // TODO: Handle this case.
        case JuceMixPlayerState.STOPPED:
        // TODO: Handle this case.
        case JuceMixPlayerState.COMPLETED:
        // TODO: Handle this case.
        case JuceMixPlayerState.ERROR:
        // TODO: Handle this case.
      }
    });

    player.setProgressHandler((progress) {
      if (!isSliderEditing) {
        setState(() => this.progress = progress);
      }
    });

    player.setErrorHandler((error) {
      log('Error: $error');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(
          content: Text(error),
          backgroundColor: Colors.red,
        ));
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Audio Player')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text('Progress: ${(progress * player.getDuration()).toStringAsFixed(2)} / ${player.getDuration().toStringAsFixed(2)}'),
            Slider(
              value: progress,
              onChanged: (value) {
                setState(() => progress = value);
                player.seek(progress);
              },
              onChangeStart: (_) {
                isSliderEditing = true;
              },
              onChangeEnd: (value) {
                isSliderEditing = false;
                player.seek(value.toDouble());
              },
            ),
            // display the current state
            Padding(
              padding: const EdgeInsets.all(8.0),
              child: Text('State: $state'),
            ),
            Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                ElevatedButton(
                  onPressed: () {
                    player.togglePlayPause();

                    setState(() => isPlaying = !isPlaying);
                  },
                  child: Text(isPlaying ? 'Pause' : 'Play'),
                ),
                const SizedBox(width: 16),
                ElevatedButton(
                  onPressed: () async {
                    final path = await AssetHelper.extractAsset('assets/media/music_big.mp3');
                    player.setFile(path);
                  },
                  child: const Text('Set File'),
                ),
                const SizedBox(width: 16),
                ElevatedButton(
                  onPressed: () async {
                    final path = await AssetHelper.extractAsset('assets/media/music_big.mp3');
                    player.setMixData(MixerData(
                      outputDuration: 150,
                      tracks: [
                        MixerTrack(id: "0", path: path),
                        MixerTrack(id: "1", path: path, offset: 0.5),
                      ],
                    ));
                  },
                  child: const Text('Set mixed'),
                ),
                const SizedBox(width: 16),
                ElevatedButton(
                  onPressed: () async {
                    player.setMixData(await createMetronomeTracks());
                  },
                  child: const Text('Set mixed with metronome'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Future<MixerData> createMetronomeTracks() async {
    final path = await AssetHelper.extractAsset('assets/media/music_big.mp3');
    final pathH = await AssetHelper.extractAsset('assets/media/met_h.wav');
    final pathL = await AssetHelper.extractAsset('assets/media/met_l.wav');
    double metVol = 0.1;
    return MixerData(
      tracks: [
        MixerTrack(id: "music", path: path),
        MixerTrack(id: "met_1", path: pathH, offset: 0, repeat: true, repeatInterval: 2, volume: metVol),
        MixerTrack(id: "met_2", path: pathL, offset: 0.5, repeat: true, repeatInterval: 2, volume: metVol),
        MixerTrack(id: "met_3", path: pathL, offset: 1, repeat: true, repeatInterval: 2, volume: metVol),
        MixerTrack(id: "met_4", path: pathL, offset: 1.5, repeat: true, repeatInterval: 2, volume: metVol)
      ],
    );
  }

  @override
  void dispose() {
    player.dispose();
    super.dispose();
  }
}
