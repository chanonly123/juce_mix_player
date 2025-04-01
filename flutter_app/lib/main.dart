import 'package:flutter/material.dart';
import 'package:flutter_app/home_page.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  JuceMixPlayer.juce_init();
  JuceMixPlayer.enableLogs(true);
  runApp(const MyApp());
  Permission.microphone.request().then((_) {});
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: HomePage(),
    );
  }
}