import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'dart:typed_data';
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

  /// Returns the PNG image bytes from the native buffer as a Uint8List.
  Uint8List getImageFromBuffer() {
    final Pointer<Void> rawPtr =
        JuceMixPlayer.juceLib.LatencyCalc_getImageBufferPointer(_ptr);
    final int size =
        JuceMixPlayer.juceLib.LatencyCalc_getImageBufferPointerSize(_ptr);
    final Pointer<Uint8> ptr = rawPtr.cast<Uint8>();
    final nativeBytes = ptr.asTypedList(size);
    final dartCopy = Uint8List.fromList(nativeBytes);
    return dartCopy;
  }

  String setDevSettings(String option) {
    return JuceMixPlayer.juceLib
        .LatencyCalc_setDevSettings(_ptr, option.toNativeUtf8())
        .toDartString();
  }

  void dispose() {
    // Clear callbacks
    _latencyFoundCallbackNativeCallable?.close();
    JuceMixPlayer.juceLib.LatencyCalc_deinit(_ptr);
  }
}
