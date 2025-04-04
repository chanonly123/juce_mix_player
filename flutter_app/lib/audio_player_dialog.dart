import 'package:flutter/material.dart';
import 'package:juce_mix_player/juce_mix_player.dart';

class AudioPlayerDialog extends StatefulWidget {
  final String filePath;
  final VoidCallback onOkPressed;

  const AudioPlayerDialog({
    super.key,
    required this.filePath,
    required this.onOkPressed,
  });

  @override
  AudioPlayerDialogState createState() => AudioPlayerDialogState();
}

class AudioPlayerDialogState extends State<AudioPlayerDialog> {
  final player = JuceMixPlayer(record: false, play: true);
  double progress = 0.0;
  bool isPlaying = false;
  bool isPlayerReady = false;
  bool isSliderEditing = false;
  JuceMixPlayerState playerState = JuceMixPlayerState.IDLE;

  @override
  void initState() {
    super.initState();

    player.setStateUpdateHandler((state) {
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
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('Player error: $error'),
        backgroundColor: Colors.red,
      ));
    });

    player.setFile(widget.filePath);
    // player.setProgressHandler((p) => setState(() => progress = p));

    player.setProgressHandler((progress) {
      if (!isSliderEditing) {
        setState(() => this.progress = progress);
      }
    });
  }

  @override
  void dispose() {
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
          Text('Progress: ${(progress * player.getDuration()).toStringAsFixed(2)} / ${player.getDuration().toStringAsFixed(2)}'),
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
                isPlaying ? player.pause() : player.play();
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
