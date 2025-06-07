import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/utils.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:share_plus/share_plus.dart';

class AudioPlayerDialog extends StatefulWidget {
  final String filePath;
  final VoidCallback onOkPressed;
  final String latencyInfo;

  const AudioPlayerDialog({
    super.key,
    required this.filePath,
    required this.latencyInfo,
    required this.onOkPressed,
  });

  @override
  AudioPlayerDialogState createState() => AudioPlayerDialogState();
}

class AudioPlayerDialogState extends State<AudioPlayerDialog> {
  final player = JuceMixPlayer();
  double progress = 0.0;
  bool isPlaying = false;
  bool isPlayerReady = false;
  bool isSliderEditing = false;
  JuceMixPlayerState playerState = JuceMixPlayerState.IDLE;

  @override
  void initState() {
    super.initState();

    setupPlayer();

    player.setStateUpdateHandler((state) {
      if (!mounted) return; // Check if widget is still mounted
      setState(() {
        playerState = state;
        switch (state) {
          case JuceMixPlayerState.PAUSED:
            setState(() => isPlaying = false);
            break;
          case JuceMixPlayerState.PLAYING:
            setState(() => isPlaying = true);
            break;
          case JuceMixPlayerState.IDLE:
            break;
          case JuceMixPlayerState.READY:
            setState(() => isPlayerReady = true);
            break;
          case JuceMixPlayerState.STOPPED:
            setState(() => isPlaying = false);
            break;
          case JuceMixPlayerState.COMPLETED:
            break;
          case JuceMixPlayerState.ERROR:
            break;
        }
      });
    });

    player.setErrorHandler((error) {
      if (!mounted) return; // Check if widget is still mounted
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('Player error: $error'),
        backgroundColor: Colors.red,
      ));
    });

    player.setProgressHandler((progress) {
      if (!isSliderEditing) {
        setState(() => this.progress = progress);
      }
    });
  }

  void setupPlayer() async {
    final beats = await AssetHelper.extractAsset('assets/media/beats.wav');
    final pathH = await AssetHelper.extractAsset('assets/media/met_h.wav');
    final pathL = await AssetHelper.extractAsset('assets/media/met_l.wav');
    double metVol = 0.5;
    final mixComposeModel = MixerComposeModel(
      tracks: [
        MixerTrack(id: "beats", path: beats, volume: metVol),
        MixerTrack(id: "music", path: widget.filePath, volume: metVol),
        // MixerTrack(id: "met_1", path: pathH, offset: 0, repeat: true, repeatInterval: 2, volume: metVol),
        // MixerTrack(id: "met_2", path: pathL, offset: 0.5, repeat: true, repeatInterval: 2, volume: metVol),
        // MixerTrack(id: "met_3", path: pathL, offset: 1, repeat: true, repeatInterval: 2, volume: metVol),
        // MixerTrack(id: "met_4", path: pathL, offset: 1.5, repeat: true, repeatInterval: 2, volume: metVol)
      ],
    );
    player.setMixData(mixComposeModel);
  }

  @override
  void dispose() {
    player.stop();
    player.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Play Recording'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(
              '${TimeUtils.formatDuration(progress * player.getDuration())} / ${TimeUtils.formatDuration(player.getDuration())}'),
          if (!isPlayerReady) const CircularProgressIndicator(),
          if (isPlayerReady)
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
                }),
          const SizedBox(height: 16),
          if (isPlayerReady)
            IconButton(
              icon: Icon(isPlaying ? Icons.pause : Icons.play_arrow),
              onPressed: () {
                player.togglePlayPause();
              },
              iconSize: 36,
            ),
          ElevatedButton(
            onPressed: () {
              SharePlus.instance.share(ShareParams(files: [XFile(widget.filePath)]));
            },
            child: Text("Share File"),
          ),
          Text("${widget.latencyInfo}"),
          ElevatedButton(
            onPressed: () {
              SharePlus.instance.share(ShareParams(text: widget.latencyInfo));
            },
            child: Text("Share Info"),
          )
        ],
      ),
      actions: [
        TextButton(
          onPressed: () {
            widget.onOkPressed();
            Navigator.of(context).pop();
          },
          child: const Text('OK'),
        ),
      ],
    );
  }
}
