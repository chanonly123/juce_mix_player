import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:juce_mix_player/juce_mix_player.dart';

class GstPlayer {
  late Pointer<Void> _ptr;

  GstPlayer() {
    _ptr = JuceMixPlayer.getJuceLib().GstPlayer_init();
  }

  bool setUrl(String url) {
    var success =
        JuceMixPlayer.getJuceLib().GstPlayer_setURL(_ptr, url.toNativeUtf8());
    return success == 1;
  }

  void stop() {
    JuceMixPlayer.getJuceLib().GstPlayer_stop(_ptr);
  }

  void dispose() {
    JuceMixPlayer.getJuceLib().GstPlayer_dispose(_ptr);
  }
}
