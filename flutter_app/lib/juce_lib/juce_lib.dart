// ignore_for_file: constant_identifier_names

import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_app/juce_lib/juce_lib_gen.dart';

// Native function typedefs
typedef StringUpdateCallback = Void Function(Pointer<Void>, Pointer<Utf8>);
typedef FloatCallback = Void Function(Pointer<Void>, Float);

// Dart function typedefs
typedef ProgressCallbackDart = void Function(double progress);
typedef StateUpdateCallbackDart = void Function(JuceMixPlayerState state);
typedef ErrorCallbackDart = void Function(String error);

final _progressCallbacks = <Pointer<Void>, ProgressCallbackDart>{};
final _stateCallbacks = <Pointer<Void>, StateUpdateCallbackDart>{};
final _errorCallbacks = <Pointer<Void>, ErrorCallbackDart>{};

enum JuceMixPlayerState { IDLE, READY, PLAYING, PAUSED, STOPPED, COMPLETED, ERROR }

class JuceLib extends JuceLibGen {
  JuceLib() : super(defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open("libjuce_lib.so"));
}

class JuceMixPlayer {
  final JuceLib _juceLib = JuceLib();
  late Pointer<Void> _ptr;

  late final NativeCallable<FloatCallback> _progressCallbackNativeCallable;
  late final NativeCallable<StringUpdateCallback> _stateUpdateNativeCallable;
  late final NativeCallable<StringUpdateCallback> _errorUpdateNativeCallable;

  JuceMixPlayer() {
    _ptr = _juceLib.JuceMixPlayer_init();

    _progressCallbackNativeCallable = NativeCallable<FloatCallback>.listener(_onProgressCallback);
    _stateUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(_onStateUpdateCallback);
    _errorUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(_onErrorCallback);

    _juceLib.JuceMixPlayer_onProgress(_ptr, _progressCallbackNativeCallable.nativeFunction);
    _juceLib.JuceMixPlayer_onStateUpdate(_ptr, _stateUpdateNativeCallable.nativeFunction);
    _juceLib.JuceMixPlayer_onError(_ptr, _errorUpdateNativeCallable.nativeFunction);
  }

  void _onStateUpdateCallback(Pointer<Void> player, Pointer<Utf8> state) {
    final stateStr = state.cast<Utf8>().toDartString();
    _stateCallbacks[player]?.call(JuceMixPlayerState.values.byName(stateStr));
  }

  void _onProgressCallback(Pointer<Void> player, double progress) {
    _progressCallbacks[player]?.call(progress);
  }

  void _onErrorCallback(Pointer<Void> player, Pointer<Utf8> error) {
    final errorStr = error.cast<Utf8>().toDartString();
    _errorCallbacks[player]?.call(errorStr);
  }

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

  void togglePlayPause() {
    if (isPlaying()) {
      pause();
    } else {
      play();
    }
  }

  void dispose() {
    // Clear callbacks
    _progressCallbacks.remove(_ptr);
    _stateCallbacks.remove(_ptr);
    _errorCallbacks.remove(_ptr);

    _progressCallbackNativeCallable.close();
    _stateUpdateNativeCallable.close();
    _errorUpdateNativeCallable.close();

    _juceLib.JuceMixPlayer_deinit(_ptr);
  }
}
