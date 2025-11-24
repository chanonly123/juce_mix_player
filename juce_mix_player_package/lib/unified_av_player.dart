// ignore_for_file: constant_identifier_names, always_use_package_imports

import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'juce_lib_gen.dart';
import 'juce_mix_player.dart'; // For model classes and enums

/// UnifiedAVPlayer Controller
/// Coordinates synchronized playback of audio (primary/master) and video (secondary/optional)
/// Audio is the master timeline, video is automatically muted and synced
class UnifiedAVPlayerController {
  static late JuceLibGen _juceLib;
  late Pointer<Void> _ptr;

  // Native callbacks
  NativeCallable<FloatCallback>? _progressCallbackNativeCallable;
  NativeCallable<StringUpdateCallback>? _stateUpdateNativeCallable;
  NativeCallable<StringUpdateCallback>? _errorUpdateNativeCallable;

  // Device callbacks
  NativeCallable<StringUpdateCallback>? _deviceUpdateNativeCallable;

  // Export callbacks
  NativeCallable<StringUpdateCallback2>? _exportAudioUpdateNativeCallable;
  NativeCallable<StringUpdateCallback2>? _exportVideoUpdateNativeCallable;

  static var libname = 'libjuce_jni.so';

  static void juce_init() {
    _juceLib = JuceLibGen(
        defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(libname));
    _juceLib.juce_init();
  }

  static JuceLibGen getJuceLib() {
    return _juceLib;
  }

  Pointer<Void> getPtr() {
    return _ptr;
  }

  static void enableLogs(bool enable) {
    _juceLib.juce_enableLogs(enable ? 1 : 0);
  }

  /// Create UnifiedAVPlayer instance (singleton pattern)
  UnifiedAVPlayerController() {
    _juceLib = JuceLibGen(
        defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(libname));

    _ptr = _juceLib.UnifiedAVPlayer_getInstance();
  }

  // ========== Unified Playback Controls ==========

  void play() {
    _juceLib.UnifiedAVPlayer_play(_ptr);
  }

  void pause() {
    _juceLib.UnifiedAVPlayer_pause(_ptr);
  }

  void stop() {
    _juceLib.UnifiedAVPlayer_stop(_ptr);
  }

  void seek(double normalizedPos) {
    _juceLib.UnifiedAVPlayer_seek(_ptr, normalizedPos);
  }

  void togglePlayPause() {
    _juceLib.UnifiedAVPlayer_togglePlayPause(_ptr);
  }

  // ========== Audio Setup (Required - Primary Media) ==========

  void setAudioData(MixerComposeModel data) {
    final jsonStr = json.encode(data.toJson());
    _juceLib.UnifiedAVPlayer_setAudioData(_ptr, jsonStr.toNativeUtf8());
  }

  void setAudioFile(String path) {
    MixerComposeModel data = MixerComposeModel(tracks: [
      MixerTrack(id: "id_0", path: path),
    ]);
    setAudioData(data);
  }

  void setAudioSettings(MixerSettings settings) {
    final jsonStr = json.encode(settings.toJson());
    _juceLib.UnifiedAVPlayer_setAudioSettings(_ptr, jsonStr.toNativeUtf8());
  }

  void resetAudioPlayBuffer() {
    _juceLib.UnifiedAVPlayer_resetAudioPlayBuffer(_ptr);
  }

  Future<void> exportAudio(String outputPath) async {
    final completer = Completer<void>();

    NativeStringCallbackDart2 closure = (cstring) {
      String error = cstring.toDartString();
      if (error.isNotEmpty) {
        completer.completeError(Exception('Audio export failed: $error'));
      } else {
        completer.complete();
      }
    };

    _exportAudioUpdateNativeCallable?.close();
    _exportAudioUpdateNativeCallable = NativeCallable<StringUpdateCallback2>.listener(closure);
    _juceLib.UnifiedAVPlayer_exportAudio(
        _ptr, outputPath.toNativeUtf8(), _exportAudioUpdateNativeCallable!.nativeFunction);

    return completer.future;
  }

  // ========== Video Setup (Optional - Secondary Media) ==========

  void setVideoPath(String path) {
    _juceLib.UnifiedAVPlayer_setVideoPath(_ptr, path.toNativeUtf8());
  }

  void setVideoSurfaceHandle(Pointer<Void> handle) {
    _juceLib.UnifiedAVPlayer_setVideoSurfaceHandle(_ptr, handle);
  }

  void setVideoRotation(int degrees) {
    _juceLib.UnifiedAVPlayer_setVideoRotation(_ptr, degrees);
  }

  void setVideoVisualEffect(int effectId) {
    _juceLib.UnifiedAVPlayer_setVideoVisualEffect(_ptr, effectId);
  }

  Future<void> exportVideo(String outputPath) async {
    final completer = Completer<void>();

    NativeStringCallbackDart2 closure = (cstring) {
      String error = cstring.toDartString();
      if (error.isNotEmpty) {
        completer.completeError(Exception('Video export failed: $error'));
      } else {
        completer.complete();
      }
    };

    _exportVideoUpdateNativeCallable?.close();
    _exportVideoUpdateNativeCallable = NativeCallable<StringUpdateCallback2>.listener(closure);
    _juceLib.UnifiedAVPlayer_exportVideo(
        _ptr, outputPath.toNativeUtf8(), _exportVideoUpdateNativeCallable!.nativeFunction);

    return completer.future;
  }

  // ========== State Queries ==========

  double getDuration() {
    return _juceLib.UnifiedAVPlayer_getDuration(_ptr);
  }

  double getCurrentTime() {
    return _juceLib.UnifiedAVPlayer_getCurrentTime(_ptr);
  }

  bool isPlaying() {
    return _juceLib.UnifiedAVPlayer_isPlaying(_ptr) == 1;
  }

  String getCurrentState() {
    return _juceLib.UnifiedAVPlayer_getCurrentState(_ptr).toDartString();
  }

  bool hasVideoLoaded() {
    return _juceLib.UnifiedAVPlayer_hasVideoLoaded(_ptr) == 1;
  }

  // ========== Callbacks (Unified - Audio Timeline Driven) ==========

  void setProgressHandler(void Function(double progress) callback) {
    FloatCallbackDart closure = (ptr, progress) {
      callback(progress);
    };
    _progressCallbackNativeCallable?.close();
    _progressCallbackNativeCallable = NativeCallable<FloatCallback>.listener(closure);
    _juceLib.UnifiedAVPlayer_onProgress(_ptr, _progressCallbackNativeCallable!.nativeFunction);
  }

  void setStateUpdateHandler(void Function(JuceMixPlayerState state) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      callback(JuceMixPlayerState.values.byName(cstring.toDartString()));
    };
    _stateUpdateNativeCallable?.close();
    _stateUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.UnifiedAVPlayer_onStateUpdate(_ptr, _stateUpdateNativeCallable!.nativeFunction);
  }

  void setErrorHandler(void Function(String error) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      callback(cstring.toDartString());
    };
    _errorUpdateNativeCallable?.close();
    _errorUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.UnifiedAVPlayer_onError(_ptr, _errorUpdateNativeCallable!.nativeFunction);
  }

  void setDeviceUpdateHandler(void Function(MixerDeviceList deviceList) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      MixerDeviceList data = MixerDeviceList.fromJson(json.decode(cstring.toDartString()));
      callback(data);
    };
    _deviceUpdateNativeCallable?.close();
    _deviceUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.UnifiedAVPlayer_onDeviceUpdate(_ptr, _deviceUpdateNativeCallable!.nativeFunction);
  }

  // ========== Cleanup ==========

  void dispose() {
    // Clear callbacks
    _progressCallbackNativeCallable?.close();
    _stateUpdateNativeCallable?.close();
    _errorUpdateNativeCallable?.close();
    _deviceUpdateNativeCallable?.close();
    _exportAudioUpdateNativeCallable?.close();
    _exportVideoUpdateNativeCallable?.close();

    _juceLib.UnifiedAVPlayer_dispose(_ptr);
  }

  /// Destroy the singleton instance (use with caution)
  static void destroyInstance() {
    _juceLib.UnifiedAVPlayer_destroyInstance();
  }
}
