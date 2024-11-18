import 'package:flutter_app/juce_lib/juce_lib.dart';
import 'package:flutter_app/juce_mix_item.dart';
import 'dart:ffi';

class JuceMixPlayer {
  final JuceLib _lib = JuceLib();
  late Pointer<Void> _ptr;

  JuceMixPlayer() {
    _ptr = _lib.JuceMixPlayer_init();
  }

  void dispose() {
    _lib.JuceMixPlayer_deinit(_ptr);
  }

  void play() {
    _lib.JuceMixPlayer_play(_ptr);
  }

  void pause() {
    _lib.JuceMixPlayer_pause(_ptr);
  }

  void addItem(JuceMixItem item) {
    _lib.JuceMixPlayer_addItem(_ptr, item.ptr);
  }
}
