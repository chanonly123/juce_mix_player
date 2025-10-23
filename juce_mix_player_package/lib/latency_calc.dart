import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:juce_mix_player/juce_mix_player.dart';

class LatencyCalc {
  late Pointer<Void> _ptr;
  NativeCallable<StringUpdateCallback>? _latencyFoundCallbackNativeCallable;

  LatencyCalc() {
    _ptr = JuceMixPlayer.juceLib.LatencyCalc_init();
  }

  void startLatencyCalculation(void Function(String level) callback) {
    NativeStringCallbackDart closure = (ptr, latency) {
      callback(latency.toDartString());
    };
    _latencyFoundCallbackNativeCallable?.close();
    _latencyFoundCallbackNativeCallable =
        NativeCallable<StringUpdateCallback>.listener(closure);
    JuceMixPlayer.juceLib.LatencyCalc_start(
        _ptr, _latencyFoundCallbackNativeCallable!.nativeFunction);
  }

  void stop() {
    JuceMixPlayer.juceLib.LatencyCalc_stop(_ptr);
  }

  void dispose() {
    // Clear callbacks
    _latencyFoundCallbackNativeCallable?.close();
    JuceMixPlayer.juceLib.LatencyCalc_deinit(_ptr);
  }
}
