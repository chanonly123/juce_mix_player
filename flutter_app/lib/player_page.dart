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
  bool isVolSliderEditing = false;
  bool isPlaying = false;
  MixerDeviceList deviceList = MixerDeviceList(devices: []);
  bool loopEnabled = false; // Add loop state variable
  double volume = 0.5; // Add volume state variable

  JuceMixPlayerState state = JuceMixPlayerState.IDLE;

  MixerComposeModel? lastMixerComposeModel;

  @override
  void initState() {
    super.initState();

    player.setSettings(MixerSettings(
      loop: loopEnabled, // Changed from hardcoded false
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
      // log('devices: ${JsonEncoder.withIndent('  ').convert(deviceList.toJson())}');
    });
  }

  void toggleLoop() {
    setState(() => loopEnabled = !loopEnabled);
    player.setSettings(MixerSettings(loop: loopEnabled));
  }

  @override
  void dispose() {
    print('PlayerPage:dispose');
    player.stop();
    player.dispose();
    super.dispose();
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
              },
              onChangeStart: (_) {
                isSliderEditing = true;
              },
              onChangeEnd: (value) {
                isSliderEditing = false;
                player.seek(value.toDouble());
              },
            ),
            // --- Volume Bar Start ---
            Row(
              children: [
                const Icon(Icons.volume_up, color: Colors.orange),
                SizedBox(width: 8),
                Expanded(
                  child: SliderTheme(
                    data: SliderTheme.of(context).copyWith(
                      activeTrackColor: Colors.orange,
                      inactiveTrackColor: Colors.orange.shade100,
                      trackHeight: 6.0,
                      thumbShape: SliderComponentShape.noThumb,
                      overlayShape: SliderComponentShape.noOverlay,
                    ),
                    child: Slider(
                      value: volume,
                      min: 0.0,
                      max: 1.0,
                      divisions: 100,
                      onChanged: (value) {
                        setState(() => volume = value);
                      },
                      onChangeStart: (_) {
                        isVolSliderEditing = true;
                      },
                      onChangeEnd: (value) {
                        isVolSliderEditing = false;
                        if (lastMixerComposeModel != null) {
                          final updatedTracks = lastMixerComposeModel!.tracks?.map((track) => track.copyWith(volume: volume)).toList() ?? [];
                          lastMixerComposeModel = lastMixerComposeModel!.copyWith(tracks: updatedTracks);
                          player.setMixData(lastMixerComposeModel!);
                        }
                      },
                    ),
                  ),
                ),
              ],
            ),
            // --- Volume Bar End ---
            // display the current state
            Padding(
              padding: const EdgeInsets.all(8.0),
              child: Text('State: $state'),
            ),
            Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Row(
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
                  ],
                ),
                const SizedBox(width: 16),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    ElevatedButton(
                      onPressed: () async {
                        final path = await AssetHelper.extractAsset('assets/media/music_big.mp3');
                        lastMixerComposeModel = MixerComposeModel(
                          tracks: [
                            MixerTrack(id: "0", path: path, volume: volume),
                          ],
                        );
                        player.setMixData(lastMixerComposeModel!);
                      },
                      child: const Text('Set File'),
                    ),
                    const SizedBox(width: 16),
                    ElevatedButton(
                      onPressed: () async {
                        final path = await AssetHelper.extractAsset('assets/media/music_big.mp3');
                        lastMixerComposeModel = MixerComposeModel(
                          outputDuration: 150,
                          tracks: [
                            MixerTrack(id: "0", path: path, volume: volume),
                            MixerTrack(id: "1", path: path, offset: 0.5, volume: volume),
                          ],
                        );
                        player.setMixData(lastMixerComposeModel!);
                      },
                      child: const Text('Set mixed'),
                    ),
                  ],
                ),
                const SizedBox(width: 16),
                ElevatedButton(
                  onPressed: () async {
                    lastMixerComposeModel = await createMetronomeTracks();
                    player.setMixData(lastMixerComposeModel!);
                  },
                  child: const Text('Set mixed with metronome'),
                ),
                const SizedBox(width: 16),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    ElevatedButton(
                      onPressed: () async {
                        try {
                          await player.export(await extractOutput);
                          print("export completed successfully");
                        } catch (e) {
                          print("export failed: $e");
                        }
                      },
                      child: const Text('Export'),
                    ),
                    const SizedBox(width: 16),
                    ElevatedButton(
                      onPressed: () async {
                        player.setFile(await extractOutput);
                      },
                      child: const Text('Play Exported file'),
                    ),
                  ],
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
    final bgmPath = await AssetHelper.extractAsset('assets/media/75_C.mp3');
    final pathH = await AssetHelper.extractAsset('assets/media/met_h.wav');
    final pathL = await AssetHelper.extractAsset('assets/media/met_l.wav');
    lastMixerComposeModel = MixerComposeModel(tracks: [
      MixerTrack(id: "bgm", path: bgmPath, volume: volume, enabled: true),
      MixerTrack(id: "metronome_track_0", path: pathH, offset: 0, volume: volume, enabled: true, repeat: true, repeatInterval: 3.2),
      MixerTrack(id: "metronome_track_1", path: pathL, offset: 0.8, volume: 1, enabled: true, repeat: true, repeatInterval: 3.2),
      MixerTrack(id: "metronome_track_2", path: pathL, offset: 1.6, volume: 1, enabled: true, repeat: true, repeatInterval: 3.2),
      MixerTrack(id: "metronome_track_3", path: pathL, offset: 2.4, volume: 1, enabled: true, repeat: true, repeatInterval: 3.2)
    ]);
    return lastMixerComposeModel!;
  }

  Future<String> get extractOutput async {
    final dirPath = await AssetHelper.getApplicationDocumentsDirectoryPath();
    return "$dirPath/out.wav";
  }
}
