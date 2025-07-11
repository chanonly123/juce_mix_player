// ignore_for_file: constant_identifier_names, always_use_package_imports

import 'dart:convert';
import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'juce_lib_gen.dart';

// Native function typedefs
typedef StringUpdateCallback = Void Function(Pointer<Void>, Pointer<Utf8>);
typedef StringUpdateCallback2 = Void Function(Pointer<Utf8>);
typedef FloatCallback = Void Function(Pointer<Void>, Float);

// Dart function typedefs
typedef FloatCallbackDart = void Function(Pointer<Void> ptr, double progress);
typedef NativeStringCallbackDart = void Function(Pointer<Void> ptr, Pointer<Utf8> state);
typedef NativeStringCallbackDart2 = void Function(Pointer<Utf8> state);

enum JuceMixPlayerState { IDLE, READY, PLAYING, PAUSED, STOPPED, COMPLETED, ERROR }

enum JuceMixRecState { IDLE, READY, RECORDING, STOPPED, ERROR }

/// initialize will fail if (record: true) and no mic permission
class JuceMixPlayer {
  static late JuceLibGen _juceLib;
  late Pointer<Void> _ptr;

  NativeCallable<FloatCallback>? _progressCallbackNativeCallable;
  NativeCallable<StringUpdateCallback>? _stateUpdateNativeCallable;
  NativeCallable<StringUpdateCallback>? _errorUpdateNativeCallable;
  NativeCallable<StringUpdateCallback>? _deviceUpdateNativeCallable;
  NativeCallable<StringUpdateCallback2>? _exportUpdateNativeCallable;

  //Rec
  NativeCallable<FloatCallback>? _recInputlevelCallbackNativeCallable;
  NativeCallable<FloatCallback>? _recRrogressCallbackNativeCallable;
  NativeCallable<StringUpdateCallback>? _recStateUpdateNativeCallable;
  NativeCallable<StringUpdateCallback>? _recErrorUpdateNativeCallable;

  static var libname = 'libjuce_jni.so';

  static void juce_init() {
    _juceLib = JuceLibGen(defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(libname));
    _juceLib.juce_init();
  }

  Pointer<Void> getPtr() {
    return _ptr;
  }

  static void enableLogs(bool enable) {
    _juceLib.juce_enableLogs(enable ? 1 : 0);
  }

  JuceMixPlayer() {
    _juceLib = JuceLibGen(defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(libname));

    _ptr = _juceLib.JuceMixPlayer_init();
  }

  void setProgressHandler(void Function(double progress) callback) {
    FloatCallbackDart closure = (ptr, progress) {
      callback(progress);
    };
    _progressCallbackNativeCallable?.close();
    _progressCallbackNativeCallable = NativeCallable<FloatCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onProgress(_ptr, _progressCallbackNativeCallable!.nativeFunction);
  }

  void setStateUpdateHandler(void Function(JuceMixPlayerState state) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      callback(JuceMixPlayerState.values.byName(cstring.toDartString()));
    };
    _stateUpdateNativeCallable?.close();
    _stateUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onStateUpdate(_ptr, _stateUpdateNativeCallable!.nativeFunction);
  }

  void setErrorHandler(void Function(String error) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      callback(cstring.toDartString());
    };
    _errorUpdateNativeCallable?.close();
    _errorUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onError(_ptr, _errorUpdateNativeCallable!.nativeFunction);
  }

  void setDeviceUpdateHandler(void Function(MixerDeviceList deviceList) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      MixerDeviceList data = MixerDeviceList.fromJson(json.decode(cstring.toDartString()));
      callback(data);
    };
    _deviceUpdateNativeCallable?.close();
    _deviceUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onDeviceUpdate(_ptr, _deviceUpdateNativeCallable!.nativeFunction);
  }

  void setFile(String path) {
    MixerComposeModel data = MixerComposeModel(tracks: [
      MixerTrack(id: "id_0", path: path),
    ]);
    final jsonStr = json.encode(data.toJson());
    _juceLib.JuceMixPlayer_set(_ptr, jsonStr.toNativeUtf8());
  }

  void setMixData(MixerComposeModel data) {
    final jsonStr = json.encode(data.toJson());
    _juceLib.JuceMixPlayer_set(_ptr, jsonStr.toNativeUtf8());
  }

  void setSettings(MixerSettings settings) {
    final jsonStr = json.encode(settings.toJson());
    _juceLib.JuceMixPlayer_setSettings(_ptr, jsonStr.toNativeUtf8());
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

  void setUpdatedDevices(MixerDeviceList devices) {
    final jsonStr = json.encode(devices.toJson());
    _juceLib.JuceMixPlayer_setUpdatedDevices(_ptr, jsonStr.toNativeUtf8());
  }

  // Recordings
  void prepareRecording(String path) {
    _juceLib.JuceMixPlayer_prepareRecorder(_ptr, path.toNativeUtf8());
  }

  void startRecording() {
    _juceLib.JuceMixPlayer_startRecorder(_ptr);
  }

  void stopRecording() {
    _juceLib.JuceMixPlayer_stopRecorder(_ptr);
  }

  void setRecErrorHandler(void Function(String error) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      callback(cstring.toDartString());
    };
    _recErrorUpdateNativeCallable?.close();
    _recErrorUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onRecError(_ptr, _recErrorUpdateNativeCallable!.nativeFunction);
  }

  void setRecStateUpdateHandler(void Function(JuceMixRecState state) callback) {
    NativeStringCallbackDart closure = (ptr, cstring) {
      callback(JuceMixRecState.values.byName(cstring.toDartString()));
    };
    _recStateUpdateNativeCallable?.close();
    _recStateUpdateNativeCallable = NativeCallable<StringUpdateCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onRecStateUpdate(_ptr, _recStateUpdateNativeCallable!.nativeFunction);
  }

  void setRecProgressHandler(void Function(double level) callback) {
    FloatCallbackDart closure = (ptr, progress) {
      callback(progress);
    };
    _recRrogressCallbackNativeCallable?.close();
    _recRrogressCallbackNativeCallable = NativeCallable<FloatCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onRecProgress(_ptr, _recRrogressCallbackNativeCallable!.nativeFunction);
  }

  void setRecLevelHandler(void Function(double level) callback) {
    FloatCallbackDart closure = (ptr, progress) {
      callback(progress);
    };
    _recInputlevelCallbackNativeCallable?.close();
    _recInputlevelCallbackNativeCallable = NativeCallable<FloatCallback>.listener(closure);
    _juceLib.JuceMixPlayer_onRecLevel(_ptr, _recInputlevelCallbackNativeCallable!.nativeFunction);
  }

  /**
    - inputLatency in millis
    - outputLatency in millis
    - bufferLatency in millis
    - sampleRate -> sample rate per second
  */
  String getDeviceLatencyInfo() {
    return _juceLib.JuceMixPlayer_getDeviceLatencyInfo(_ptr).toDartString();
  }

  void export(String outputFile, void Function(String error) callback) {
    NativeStringCallbackDart2 closure = (cstring) {
      String error = cstring.toDartString();
      callback(error);
    };
    _exportUpdateNativeCallable?.close();
    _exportUpdateNativeCallable = NativeCallable<StringUpdateCallback2>.listener(closure);
    _juceLib.JuceMixPlayer_export(_ptr, outputFile.toNativeUtf8(), _exportUpdateNativeCallable!.nativeFunction);
  }

  void dispose() {
    // Clear callbacks
    _progressCallbackNativeCallable?.close();
    _stateUpdateNativeCallable?.close();
    _errorUpdateNativeCallable?.close();
    _deviceUpdateNativeCallable?.close();
    _exportUpdateNativeCallable?.close();

    //Rec
    _recInputlevelCallbackNativeCallable?.close();
    _recRrogressCallbackNativeCallable?.close();
    _recStateUpdateNativeCallable?.close();
    _recErrorUpdateNativeCallable?.close();

    _juceLib.JuceMixPlayer_deinit(_ptr);
  }
}

class MixerComposeModel {
  List<MixerTrack>? tracks;
  String? output;
  double? outputDuration;

  MixerComposeModel({
    required this.tracks,
    this.output,
    this.outputDuration,
  });

  MixerComposeModel copyWith({
    List<MixerTrack>? tracks,
    String? output,
    double? outputDuration,
  }) {
    return MixerComposeModel(
      tracks: tracks ?? this.tracks,
      output: output ?? this.output,
      outputDuration: outputDuration ?? this.outputDuration,
    );
  }

  factory MixerComposeModel.fromJson(Map<String, dynamic> json) => MixerComposeModel(
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

  MixerTrack copyWith({
    String? id,
    String? path,
    double? volume,
    double? duration,
    double? fromTime,
    bool? enabled,
    double? offset,
    bool? repeat,
    double? repeatInterval,
  }) {
    return MixerTrack(
      id: id ?? this.id,
      path: path ?? this.path,
      volume: volume ?? this.volume,
      offset: offset ?? this.offset,
      fromTime: fromTime ?? this.fromTime,
      duration: duration ?? this.duration,
      enabled: enabled ?? this.enabled,
      repeat: repeat ?? this.repeat,
      repeatInterval: repeatInterval ?? this.repeatInterval,
    );
  }

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

class MixerDevice {
  String name;
  bool isInput;
  bool isSelected;
  List<String> inputChannelNames;
  List<String> outputChannelNames;
  double currentSampleRate;
  List<double> availableSampleRates;
  String deviceType;

  MixerDevice({
    required this.name,
    required this.isInput,
    required this.isSelected,
    this.inputChannelNames = const [],
    this.outputChannelNames = const [],
    this.currentSampleRate = 0.0,
    this.availableSampleRates = const [],
    this.deviceType = '',
  });

  factory MixerDevice.fromJson(Map<String, dynamic> json) => MixerDevice(
        name: json['name'],
        isInput: json['isInput'],
        isSelected: json['isSelected'],
        inputChannelNames: (json['inputChannelNames'] as List<dynamic>?)?.map((e) => e.toString()).toList() ?? [],
        outputChannelNames: (json['outputChannelNames'] as List<dynamic>?)?.map((e) => e.toString()).toList() ?? [],
        currentSampleRate: json['currentSampleRate']?.toDouble() ?? 0.0,
        availableSampleRates: (json['availableSampleRates'] as List<dynamic>?)?.map((e) => e.toDouble()).toList().cast<double>() ?? [],
        deviceType: json['deviceType'] ?? '',
      );

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    json['name'] = name;
    json['isInput'] = isInput;
    json['isSelected'] = isSelected;
    json['inputChannelNames'] = inputChannelNames;
    json['outputChannelNames'] = outputChannelNames;
    json['currentSampleRate'] = currentSampleRate;
    json['availableSampleRates'] = availableSampleRates;
    json['deviceType'] = deviceType;
    return json;
  }
}

class MixerDeviceList {
  List<MixerDevice> devices;

  MixerDeviceList({
    required this.devices,
  });

  factory MixerDeviceList.fromJson(Map<String, dynamic> json) => MixerDeviceList(
        devices: (json['devices'] as List<dynamic>?)?.map((e) => MixerDevice.fromJson(e as Map<String, dynamic>)).toList() ?? [],
      );

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    json['devices'] = devices.map((e) => e.toJson()).toList();
    return json;
  }
}

class MixerSettings {
  /// in seconds [0.05]
  double progressUpdateInterval;

  /// in Hz [48000]
  int sampleRate;

  /// default is [true]
  bool stopRecOnPlaybackComplete;

  /// default is [false]
  bool loop;

  /// Playback recording in background [true]
  bool recBgPlayback;

  /// enableMicMonitoring [false]
  bool enableMicMonitoring;

  MixerSettings({
    this.progressUpdateInterval = 0.05,
    this.sampleRate = 48000,
    this.stopRecOnPlaybackComplete = true,
    this.loop = false,
    this.recBgPlayback = true,
    this.enableMicMonitoring = false,
  });

  factory MixerSettings.fromJson(Map<String, dynamic> json) => MixerSettings(
        progressUpdateInterval: json['progressUpdateInterval']?.toDouble() ?? 0.05,
        sampleRate: json['sampleRate'] ?? 48000,
        stopRecOnPlaybackComplete: json['stopRecOnPlaybackComplete'] ?? true,
        loop: json['loop'] ?? true,
        recBgPlayback: json['recBgPlayback'] ?? true,
        enableMicMonitoring: json['enableMicMonitoring'] ?? false,
      );

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    json['progressUpdateInterval'] = progressUpdateInterval;
    json['sampleRate'] = sampleRate;
    json['stopRecOnPlaybackComplete'] = stopRecOnPlaybackComplete;
    json['loop'] = loop;
    json['recBgPlayback'] = recBgPlayback;
    json['enableMicMonitoring'] = enableMicMonitoring;
    return json;
  }
}
