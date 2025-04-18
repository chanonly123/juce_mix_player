import 'dart:convert';
import 'dart:developer';

import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/utils.dart';
import 'package:flutter_app/widgets.dart';
import 'package:juce_mix_player/juce_mix_player.dart';

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
  MixerDeviceList deviceList = MixerDeviceList(devices: []);
  bool loopEnabled = false;  // Add loop state variable

  JuceMixPlayerState state = JuceMixPlayerState.IDLE;

  @override
  void initState() {
    super.initState();

    player.setSettings(MixerSettings(
      loop: loopEnabled,  // Changed from hardcoded false
    ));

    player.setStateUpdateHandler((state) {
      setState(() => this.state = state);
      print("setStateUpdateHandler:  ${state.toString()}");
      switch (state) {
        case JuceMixPlayerState.PAUSED:
          setState(() => isPlaying = false);
          break;
        case JuceMixPlayerState.PLAYING:
          setState(() => isPlaying = true);
          break;
        case JuceMixPlayerState.IDLE:
          // TODO: Handle this case.
          break;
        case JuceMixPlayerState.READY:
          ScaffoldMessenger.of(context).showSnackBar(SnackBar(
            content: Text('Player is ready, You can can play now!'),
            backgroundColor: Colors.green,
          ));
          break;
        case JuceMixPlayerState.STOPPED:
          // TODO: Handle this case.
          setState(() => isPlaying = false);
          break;
        case JuceMixPlayerState.COMPLETED:
          // TODO: Handle this case.
          break;
        case JuceMixPlayerState.ERROR:
          // TODO: Handle this case.
          break;
      }
    });

    player.setProgressHandler((progress) {
      // TODO: add 1 sec debounce
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

    player.setDeviceUpdateHandler((deviceList) {
      setState(() {
        this.deviceList = deviceList;
      });
      log('devices: ${JsonEncoder.withIndent('  ').convert(deviceList.toJson())}');
    });
  }

  @override
  void dispose() {
    print('PlayerPage:dispose');
    player.stop();
    player.dispose();
    super.dispose();
  }

  void toggleLoop() {
    setState(() => loopEnabled = !loopEnabled);
    player.setSettings(MixerSettings(loop: loopEnabled));
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
            Row(
              children: [
                Expanded(
                  child: Text(
                    '${TimeUtils.formatDuration(progress * player.getDuration())} / ${TimeUtils.formatDuration(player.getDuration())}',
                    textAlign: TextAlign.center,
                  ),
                ),
                IconButton(
                  icon: Icon(Icons.loop, color: loopEnabled ? Colors.blue : Colors.grey),
                  onPressed: toggleLoop,
                  tooltip: 'Toggle Loop',
                ),
              ],
            ),
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
                    if (state == JuceMixPlayerState.IDLE) {
                      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
                        content: Text('Player is not ready, Set file first!'),
                        backgroundColor: Colors.brown,
                      ));
                      return;
                    }
                    player.togglePlayPause();
                  },
                  child: Text(isPlaying ? 'Pause' : 'Play'),
                ),
                const SizedBox(width: 16),
                ElevatedButton(
                  onPressed: () {
                    player.stop();
                  },
                  child: Text(
                    "Stop",
                    style: TextStyle(color: Colors.red),
                  ),
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
                    player.setMixData(MixerComposeModel(
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
                const SizedBox(height: 30),
                ElevatedButton.icon(
                  icon: const Icon(Icons.speaker_group, size: 20),
                  label: const Text('Audio Devices'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.blue[900],
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(8),
                      side: BorderSide(color: Colors.blue.shade700, width: 1),
                    ),
                  ),
                  onPressed: () => showDialog(
                    context: context,
                    builder: (context) => DeviceListDialog(devices: deviceList.devices),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Future<MixerComposeModel> createMetronomeTracks() async {
    final path = await AssetHelper.extractAsset('assets/media/music_big.mp3');
    final pathH = await AssetHelper.extractAsset('assets/media/met_h.wav');
    final pathL = await AssetHelper.extractAsset('assets/media/met_l.wav');
    double metVol = 0.1;
    return MixerComposeModel(
      tracks: [
        MixerTrack(id: "music", path: path),
        MixerTrack(id: "met_1", path: pathH, offset: 0, repeat: true, repeatInterval: 2, volume: metVol),
        MixerTrack(id: "met_2", path: pathL, offset: 0.5, repeat: true, repeatInterval: 2, volume: metVol),
        MixerTrack(id: "met_3", path: pathL, offset: 1, repeat: true, repeatInterval: 2, volume: metVol),
        MixerTrack(id: "met_4", path: pathL, offset: 1.5, repeat: true, repeatInterval: 2, volume: metVol)
      ],
    );
  }
}
