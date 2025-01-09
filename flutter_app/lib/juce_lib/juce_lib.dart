import 'dart:ffi';

import 'package:flutter/foundation.dart';
import 'package:flutter_app/juce_lib/juce_lib_gen.dart';

class JuceLib extends JuceLibGen {
  JuceLib()
      : super(defaultTargetPlatform == TargetPlatform.iOS
            ? DynamicLibrary.process()
            : DynamicLibrary.open("libjuce_lib.so"));
}

class JuceMixPlayer {
  final JuceLib _juceLib = JuceLib();

  late Pointer<Void> _ptr;

  JuceMixPlayer() {
    _ptr = _juceLib.JuceMixPlayer_init();
  }

  void play() {
    _juceLib.JuceMixPlayer_play(_ptr);
  }
}
