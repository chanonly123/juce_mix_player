import 'package:flutter_app/native_library.dart';
import 'dart:ffi';
import "package:ffi/ffi.dart";
import 'package:flutter/foundation.dart';

class JuceMixItem {
  late NativeLibrary _lib;
  late Pointer<Void> _ptr;

  JuceMixItem() {
    _lib =
        NativeLibrary(defaultTargetPlatform == TargetPlatform.iOS ? DynamicLibrary.process() : DynamicLibrary.open(""));
    _ptr = _lib.JuceMixItem_init();
  }

  void dispose() {
    _lib.JuceMixItem_deinit(_ptr);
  }

  void setPath(String path, double begin, double end) {
    _lib.JuceMixItem_setPath(_ptr, path.toNativeUtf8(), begin, end);
  }
}
