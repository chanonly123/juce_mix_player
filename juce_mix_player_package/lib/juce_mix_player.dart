// ignore_for_file: constant_identifier_names

import 'dart:convert';
import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'juce_lib_gen.dart';

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

class JuceMixPlayer {
  late JuceLibGen _juceLib;
  late Pointer<Void> _ptr;

  late final NativeCallable<FloatCallback> _progressCallbackNativeCallable;
  late final NativeCallable<StringUpdateCallback> _stateUpdateNativeCallable;
  late final NativeCallable<StringUpdateCallback> _errorUpdateNativeCallable;

  static var libname = '';

  static void enableLogs(bool enable) {
    var _juceLib = JuceLibGen(
        defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(libname));
    _juceLib.juce_enableLogs(enable ? 1 : 0);
  }

  JuceMixPlayer({required bool record, required bool play}) {
    _juceLib = JuceLibGen(
        defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(libname));

    _ptr = _juceLib.JuceMixPlayer_init(record ? 1 : 0, play ? 1 : 0);

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
    MixerData data = MixerData(tracks: [
      MixerTrack(id: "id_0", path: path),
    ]);
    final jsonStr = json.encode(data.toJson());
    _juceLib.JuceMixPlayer_set(_ptr, jsonStr.toNativeUtf8());
  }

  void setMixData(MixerData data) {
    final jsonStr = json.encode(data.toJson());
    _juceLib.JuceMixPlayer_set(_ptr, jsonStr.toNativeUtf8());
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

class MixerData {
  List<MixerTrack>? tracks;
  String? output;
  double? outputDuration;

  MixerData({
    required this.tracks,
    this.output,
    this.outputDuration,
  });

  factory MixerData.fromJson(Map<String, dynamic> json) => MixerData(
        tracks: (json['tracks'] as List<dynamic>?)?.map((e) => MixerTrack.fromJson(e as Map<String, dynamic>)).toList(),
        output: json['output'],
        outputDuration: json['outputDuration']?.toDouble(),
      );

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    if (tracks != null) json['tracks'] = tracks?.map((e) => e.toJson()).toList();
    if (output != null) json['output'] = output;
    if (outputDuration != null) json['outputDuration'] = outputDuration;
    return json;
  }
}

class MixerTrack {
  double? duration;
  double? volume;
  double? fromTime;
  bool? enabled;
  String? path;
  double? offset;
  String id;
  bool? repeat;
  double? repeatInterval;

  MixerTrack({
    required this.id,
    required this.path,
    this.offset,
    this.fromTime,
    this.duration,
    this.volume,
    this.enabled,
    this.repeat,
    this.repeatInterval,
  });

  factory MixerTrack.fromJson(Map<String, dynamic> json) => MixerTrack(
        id: json['id_'],
        path: json['path'],
        offset: json['offset']?.toDouble(),
        fromTime: json['fromTime']?.toDouble(),
        duration: json['duration']?.toDouble(),
        volume: json['volume']?.toDouble(),
        enabled: json['enabled'],
        repeat: json['repeat'],
        repeatInterval: json['repeatInterval']?.toDouble(),
      );

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    json['id_'] = id;
    json['path'] = path;
    if (offset != null) json['offset'] = offset;
    if (fromTime != null) json['fromTime'] = fromTime;
    if (duration != null) json['duration'] = duration;
    if (volume != null) json['volume'] = volume;
    if (enabled != null) json['enabled'] = enabled;
    if (repeat != null) json['repeat'] = repeat;
    if (repeatInterval != null) json['repeatInterval'] = repeatInterval;
    return json;
  }
}
