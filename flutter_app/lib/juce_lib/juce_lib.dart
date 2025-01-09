import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_app/juce_lib/juce_lib_gen.dart';

// Dart function typedefs
typedef ProgressCallbackDart = void Function(Pointer<Void>, double);
typedef StringCallbackDart = void Function(Pointer<Void>, Pointer<Utf8>);

// Native function typedefs
typedef ProgressCallbackNative = Void Function(Pointer<Void>, Float);
typedef StringCallbackNative = Void Function(Pointer<Void>, Pointer<Utf8>);

class JuceLib extends JuceLibGen {
  JuceLib() : super(defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open("libjuce_lib.so"));
}

class JuceMixPlayer {
  final JuceLib _juceLib = JuceLib();
  late Pointer<Void> _ptr;

  JuceMixPlayer() {
    _ptr = _juceLib.JuceMixPlayer_init();
  }

  void setFile(String path) {
    final data = {
      'tracks': [
        {'path': path, 'enabled': true}
      ]
    };
    _juceLib.JuceMixPlayer_set(_ptr, jsonEncode(data).toNativeUtf8().cast());
  }

  void play() {
    _juceLib.JuceMixPlayer_play(_ptr);
  }

  void pause() {
    _juceLib.JuceMixPlayer_pause(_ptr);
  }

  void stop() {
    _juceLib.JuceMixPlayer_stop(_ptr);
  }

  bool isPlaying() {
    return _juceLib.JuceMixPlayer_isPlaying(_ptr) == 1;
  }

  double getDuration() {
    return _juceLib.JuceMixPlayer_getDuration(_ptr);
  }

  void seek(double position) {
    _juceLib.JuceMixPlayer_seek(_ptr, position);
  }

  void dispose() {
    _juceLib.JuceMixPlayer_deinit(_ptr);
  }
}
