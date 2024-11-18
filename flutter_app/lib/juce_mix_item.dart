import 'package:flutter_app/juce_lib/juce_lib.dart';
import 'dart:ffi';
import "package:ffi/ffi.dart";

class JuceMixItem {
  final JuceLib _lib = JuceLib();
  late Pointer<Void> _ptr;

  JuceMixItem() {
    _ptr = _lib.JuceMixItem_init();
  }

  Pointer<Void> get ptr => _ptr;

  void dispose() {
    _lib.JuceMixItem_deinit(_ptr);
  }

  void setPath(String path, double begin, double end) {
    _lib.JuceMixItem_setPath(_ptr, path.toNativeUtf8(), begin, end);
  }
}
