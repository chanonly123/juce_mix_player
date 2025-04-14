import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/utils.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:just_audio/just_audio.dart';

class AudioPlayerDialog extends StatefulWidget {
  final String filePath;
  final bool metronome;
  final VoidCallback onOkPressed;

  const AudioPlayerDialog({
    super.key,
    required this.filePath,
    required this.onOkPressed,
    required this.metronome,
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
    // player.setFile(widget.filePath);
    _setMixedRecFile();

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

  @override
  void dispose() {
    player.stop();
    player.dispose();
    super.dispose();
  }

  Future<double?> getAudioDuration(String url) async {
    final player = AudioPlayer();
    try {
      await player.setUrl(url); // Can also be a local file path
      return player.duration?.inSeconds.toDouble() ?? 0.0;
    } finally {
      await player.dispose();
    }
  }

  Future<void> _setMixedRecFile() async {
    final bgmPath = await AssetHelper.extractAsset('assets/media/tu_hi_re_92_D_sharp_bgm.mp3');
    final pathH = await AssetHelper.extractAsset('assets/media/met_h.wav');
    final pathL = await AssetHelper.extractAsset('assets/media/met_l.wav');
    const metVol = 0.3;
    final vocalDuration = await getAudioDuration(widget.filePath);

    List<MixerTrack> tracks = [
      MixerTrack(id: "vocal", path: widget.filePath),
      MixerTrack(id: "music", path: bgmPath),
    ];
    if (widget.metronome) {
      tracks.add(MixerTrack(id: "met_1", path: pathH, offset: 0, repeat: true, repeatInterval: 2, volume: metVol));
      tracks.add(MixerTrack(id: "met_2", path: pathL, offset: 0.5, repeat: true, repeatInterval: 2, volume: metVol));
      tracks.add(MixerTrack(id: "met_3", path: pathL, offset: 1, repeat: true, repeatInterval: 2, volume: metVol));
      tracks.add(MixerTrack(id: "met_4", path: pathL, offset: 1.5, repeat: true, repeatInterval: 2, volume: metVol));
    }

    player.setMixData(MixerComposeModel(tracks: tracks, outputDuration: vocalDuration));
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Play Recording'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text('${TimeUtils.formatDuration(progress * player.getDuration())} / ${TimeUtils.formatDuration(player.getDuration())}'),
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
