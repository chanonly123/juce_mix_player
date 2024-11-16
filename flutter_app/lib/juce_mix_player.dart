import 'package:flutter_app/native_library.dart';
import 'dart:ffi';
import 'package:flutter/foundation.dart';

class JuceMixPlayer {
  late NativeLibrary _lib;
  late Pointer<Void> _ptr;

  JuceMixPlayer() {
    _lib =
        NativeLibrary(defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(""));
    _lib.juceEnableLogs();
    _ptr = _lib.JuceMixPlayer_init();
  }

  void play() {
    _lib.JuceMixPlayer_play(_ptr);
  }

  void pause() {
    _lib.JuceMixPlayer_pause(_ptr);
  }

  void dispose() {
    _lib.JuceMixPlayer_deinit(_ptr);
  }
}
