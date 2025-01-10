import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_app/juce_lib/juce_lib_gen.dart';

// Dart function typedefs
typedef ProgressCallbackDart = void Function(double progress);
typedef StateUpdateCallbackDart = void Function(String state);
typedef ErrorCallbackDart = void Function(String error);

// Native function typedefs
typedef ProgressCallbackNative = Void Function(Pointer<Void>, Float);
typedef StateUpdateCallbackNative = Void Function(Pointer<Void>, Pointer<Utf8>);
typedef ErrorCallbackNative = Void Function(Pointer<Void>, Pointer<Utf8>);

// Native callbacks storage
final _progressCallbacks = <Pointer<Void>, ProgressCallbackDart>{};
final _stateCallbacks = <Pointer<Void>, StateUpdateCallbackDart>{};
final _errorCallbacks = <Pointer<Void>, ErrorCallbackDart>{};

class JuceLib extends JuceLibGen {
  JuceLib() : super(defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open("libjuce_lib.so"));
}

class JuceMixPlayer {
  final JuceLib _juceLib = JuceLib();
  late Pointer<Void> _ptr;

  JuceMixPlayer() {
    _ptr = _juceLib.JuceMixPlayer_init();

    // Set up callbacks
    _juceLib.JuceMixPlayer_onProgress(_ptr, Pointer.fromFunction<ProgressCallbackNative>(_onProgressCallback));
    _juceLib.JuceMixPlayer_onStateUpdate(_ptr, Pointer.fromFunction<StateUpdateCallbackNative>(_onStateUpdateCallback));
  }

  // Static callback handlers
  static void _onProgressCallback(Pointer<Void> ptr, double progress) {
    _progressCallbacks[ptr]?.call(progress);
  }

  static void _onStateUpdateCallback(Pointer<Void> ptr, Pointer<Utf8> state) {
    final stateStr = state.toDartString();
    _stateCallbacks[ptr]?.call(stateStr);
  }

  // Public callback setters
  void setProgressHandler(ProgressCallbackDart callback) {
    _progressCallbacks[_ptr] = callback;
  }

  void setStateUpdateHandler(StateUpdateCallbackDart callback) {
    _stateCallbacks[_ptr] = callback;
  }

  void setErrorHandler(ErrorCallbackDart callback) {
    _errorCallbacks[_ptr] = callback;
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
    // Clear callbacks
    _progressCallbacks.remove(_ptr);
    _stateCallbacks.remove(_ptr);
    _errorCallbacks.remove(_ptr);

    _juceLib.JuceMixPlayer_deinit(_ptr);
  }
}
