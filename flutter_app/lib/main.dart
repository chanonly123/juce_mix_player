import 'dart:convert';
import 'dart:developer';

import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:permission_handler/permission_handler.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  JuceMixPlayer.juce_init();
  JuceMixPlayer.enableLogs(true);
  runApp(const MyApp());
  await Permission.microphone.request();
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
  final player = JuceMixPlayer(record: false, play: true);
  double progress = 0.0;
  bool isSliderEditing = false;
  bool isPlaying = false;
  MixerDeviceList deviceList = MixerDeviceList(devices: []);

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

    player.setDeviceUpdateHandler((deviceList) {
      setState(() {
        this.deviceList = deviceList;
      });
      log('devices: ${JsonEncoder.withIndent('  ').convert(deviceList.toJson())}');
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
                const SizedBox(width: 16),
                popupMenu(),
              ],
            ),
          ],
        ),
      ),
    );
  }

  PopupMenuButton popupMenu() {
    return PopupMenuButton<MixerDevice>(
      child: Text("DEVICES: ${deviceList.devices.length}"),
      itemBuilder: (context) => deviceList.devices.map((dev) {
        return PopupMenuItem<MixerDevice>(
          value: dev,
          child: Text(getName(dev)),
        );
      }).toList(),
      onSelected: (selectedDevice) {
        deviceList.devices.forEach((d) {
          d.isSelected = false;
        });
        selectedDevice.isSelected = true;
        player.setUpdatedDevices(deviceList);
      },
    );
  }

  String getName(MixerDevice device) {
    return "${device.name} ${device.isSelected ? " ✅" : ""}";
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

  @override
  void dispose() {
    player.dispose();
    super.dispose();
  }
}
