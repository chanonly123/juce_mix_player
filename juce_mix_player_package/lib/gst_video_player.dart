// ignore_for_file: constant_identifier_names, always_use_package_imports

import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';

// Native function typedefs
typedef _StringUpdateCallback = Void Function(Pointer<Void>, Pointer<Utf8>);
typedef _FloatCallback = Void Function(Pointer<Void>, Float);

typedef _init_t = Pointer<Void> Function();
typedef _deinit_t = Void Function(Pointer<Void>);
typedef _setVideoPath_t = Void Function(Pointer<Void>, Pointer<Utf8>);
typedef _play_t = Void Function(Pointer<Void>);
typedef _pause_t = Void Function(Pointer<Void>);
typedef _stop_t = Void Function(Pointer<Void>);
typedef _seek_t = Void Function(Pointer<Void>, Float);
typedef _isPlaying_t = Int32 Function(Pointer<Void>);
typedef _getDuration_t = Float Function(Pointer<Void>);

typedef _onStateUpdate_t = Void Function(
    Pointer<Void>, Pointer<NativeFunction<_StringUpdateCallback>>);
typedef _onProgress_t = Void Function(
    Pointer<Void>, Pointer<NativeFunction<_FloatCallback>>);
typedef _onError_t = Void Function(
    Pointer<Void>, Pointer<NativeFunction<_StringUpdateCallback>>);

typedef _setSurfaceHandle_t = Void Function(Pointer<Void>, Pointer<Void>);
typedef _setMuteEmbeddedAudio_t = Void Function(Pointer<Void>, Int32);

// Dart callback typedefs
// typedef _StringUpdateCallbackDart = void Function(Pointer<Void>, Pointer<Utf8>);
// typedef _FloatCallbackDart = void Function(Pointer<Void>, double);

// Dart FFI function typedefs
typedef _deinit_dart_t = void Function(Pointer<Void>);
typedef _setVideoPath_dart_t = void Function(Pointer<Void>, Pointer<Utf8>);
typedef _play_dart_t = void Function(Pointer<Void>);
typedef _pause_dart_t = void Function(Pointer<Void>);
typedef _stop_dart_t = void Function(Pointer<Void>);
typedef _seek_dart_t = void Function(Pointer<Void>, double);
typedef _isPlaying_dart_t = int Function(Pointer<Void>);
typedef _getDuration_dart_t = double Function(Pointer<Void>);

typedef _onStateUpdate_dart_t = void Function(
    Pointer<Void>, Pointer<NativeFunction<_StringUpdateCallback>>);
typedef _onProgress_dart_t = void Function(
    Pointer<Void>, Pointer<NativeFunction<_FloatCallback>>);
typedef _onError_dart_t = void Function(
    Pointer<Void>, Pointer<NativeFunction<_StringUpdateCallback>>);
typedef _setSurfaceHandle_dart_t = void Function(Pointer<Void>, Pointer<Void>);
typedef _setMuteEmbeddedAudio_dart_t = void Function(Pointer<Void>, int);

class GstPlayerController {
  // Open the same native library as JuceMixPlayer uses
  static DynamicLibrary _openLib() =>
      defaultTargetPlatform == TargetPlatform.iOS
          ? DynamicLibrary.process()
          : DynamicLibrary.open('libjuce_jni.so');

  late final DynamicLibrary _lib;

  late final Pointer<Void> _ptr;

  // Local state tracking since native functions don't exist yet
  bool _isMuted = false;
  double _currentPosition = 0.0;
  double _duration = 0.0;

  // native function lookups
  late final _init_t _init;
  late final _deinit_dart_t _deinit;
  late final _setVideoPath_dart_t _setVideoPath;
  late final _play_dart_t _play;
  late final _pause_dart_t _pause;
  late final _stop_dart_t _stop;
  late final _seek_dart_t _seek;
  late final _isPlaying_dart_t _isPlaying;
  late final _getDuration_dart_t _getDuration;

  late final _onStateUpdate_dart_t _onStateUpdate;
  late final _onProgress_dart_t _onProgress;
  late final _onError_dart_t _onError;
  late final _setSurfaceHandle_dart_t _setSurfaceHandle;
  late final _setMuteEmbeddedAudio_dart_t _setMuteEmbeddedAudio;

  NativeCallable<_FloatCallback>? _progressCallback;
  NativeCallable<_StringUpdateCallback>? _stateCallback;
  NativeCallable<_StringUpdateCallback>? _errorCallback;

  GstPlayerController() {
    _lib = _openLib();

    _init = _lib.lookupFunction<_init_t, _init_t>('GstPlayer_init');
    _deinit =
        _lib.lookupFunction<_deinit_t, _deinit_dart_t>('GstPlayer_deinit');
    _setVideoPath = _lib.lookupFunction<_setVideoPath_t, _setVideoPath_dart_t>(
        'GstPlayer_setVideoPath');
    _play = _lib.lookupFunction<_play_t, _play_dart_t>('GstPlayer_play');
    _pause = _lib.lookupFunction<_pause_t, _pause_dart_t>('GstPlayer_pause');
    _stop = _lib.lookupFunction<_stop_t, _stop_dart_t>('GstPlayer_stop');
    _seek = _lib.lookupFunction<_seek_t, _seek_dart_t>('GstPlayer_seek');
    _isPlaying = _lib
        .lookupFunction<_isPlaying_t, _isPlaying_dart_t>('GstPlayer_isPlaying');
    _getDuration = _lib.lookupFunction<_getDuration_t, _getDuration_dart_t>(
        'GstPlayer_getDuration');
    _onStateUpdate =
        _lib.lookupFunction<_onStateUpdate_t, _onStateUpdate_dart_t>(
            'GstPlayer_onStateUpdate');
    _onProgress = _lib.lookupFunction<_onProgress_t, _onProgress_dart_t>(
        'GstPlayer_onProgress');
    _onError =
        _lib.lookupFunction<_onError_t, _onError_dart_t>('GstPlayer_onError');

    _setSurfaceHandle =
        _lib.lookupFunction<_setSurfaceHandle_t, _setSurfaceHandle_dart_t>(
            'GstPlayer_setSurfaceHandle');
    _setMuteEmbeddedAudio = _lib.lookupFunction<_setMuteEmbeddedAudio_t,
        _setMuteEmbeddedAudio_dart_t>('GstPlayer_setMuteEmbeddedAudio');

    _ptr = _init();
  }

  void setVideoPath(String path) {
    _setVideoPath(_ptr, path.toNativeUtf8());
  }

  void play() => _play(_ptr);
  void pause() => _pause(_ptr);
  void stop() => _stop(_ptr);

  void seek(double normalized) => _seek(_ptr, normalized.toDouble());

  bool isPlaying() => _isPlaying(_ptr) == 1;

  double getDuration() => _getDuration(_ptr);

  double getCurrentPosition() => _currentPosition;

  bool isMuted() => _isMuted;

  void setMuted(bool muted) {
    _isMuted = muted;
    // Use existing setMuteEmbeddedAudio for now
    setMuteEmbeddedAudio(muted);
  }

  void toggleMute() => setMuted(!isMuted());

  void togglePlayPause() {
    if (isPlaying()) {
      pause();
    } else {
      play();
    }
  }

  void setMuteEmbeddedAudio(bool mute) =>
      _setMuteEmbeddedAudio(_ptr, mute ? 1 : 0);

  void setSurfaceHandle(Pointer<Void> nativeSurface) =>
      _setSurfaceHandle(_ptr, nativeSurface);

  // Expose native pointer address for PlatformView handoff
  int get nativeHandle => _ptr.address;

  void setProgressHandler(void Function(double normalized) callback) {
    _progressCallback?.close();
    _progressCallback = NativeCallable<_FloatCallback>.listener(
        (Pointer<Void> ptr, double value) {
      // Update local state
      _duration = getDuration();
      _currentPosition = value * _duration;
      callback(value);
    });
    _onProgress(_ptr, _progressCallback!.nativeFunction);
  }

  void setStateUpdateHandler(void Function(String state) callback) {
    _stateCallback?.close();
    _stateCallback = NativeCallable<_StringUpdateCallback>.listener(
        (Pointer<Void> ptr, Pointer<Utf8> cstr) {
      callback(cstr.toDartString());
    });
    _onStateUpdate(_ptr, _stateCallback!.nativeFunction);
  }

  void setErrorHandler(void Function(String error) callback) {
    _errorCallback?.close();
    _errorCallback = NativeCallable<_StringUpdateCallback>.listener(
        (Pointer<Void> ptr, Pointer<Utf8> cstr) {
      callback(cstr.toDartString());
    });
    _onError(_ptr, _errorCallback!.nativeFunction);
  }

  void dispose() {
    _progressCallback?.close();
    _stateCallback?.close();
    _errorCallback?.close();
    _deinit(_ptr);
  }
}
