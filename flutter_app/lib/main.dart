import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/juce_lib/juce_lib.dart';
// import 'package:flutter_app/juce_mix_item.dart';
// import 'package:flutter_app/juce_mix_player.dart';

void main() async {
  JuceLib().juceEnableLogs();
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: ButtonScreen(),
    );
  }
}

class ButtonScreen extends StatefulWidget {
  const ButtonScreen({super.key});

  @override
  ButtonScreenState createState() => ButtonScreenState();
}

class ButtonScreenState extends State<ButtonScreen> {
  // JuceMixPlayer? player;
  // JuceMixItem? item;

  final JuceMixPlayer player = JuceMixPlayer();

  ButtonScreenState();

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Flutter Stateful Buttons'),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            ElevatedButton(
              onPressed: () async {
                // item ??= JuceMixItem();
                // player ??= JuceMixPlayer();

                String path =
                    await AssetHelper.extractAsset('assets/media/music.mp3');
                // item?.setPath(path, 0, 0);

                // player?.addItem(item!);
              },
              child: Text('Open file'),
            ),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: () {
                // player?.play();
              },
              child: Text('Play'),
            ),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: () {
                // player?.pause();
              },
              child: Text('Pause'),
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    // player?.dispose();
    // item?.dispose();
    super.dispose();
  }
}
