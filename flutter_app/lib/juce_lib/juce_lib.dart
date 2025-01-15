import 'dart:ffi';
import 'juce_lib_gen.dart';
import 'package:flutter/foundation.dart';

class JuceLib extends JuceLibGen {
  JuceLib()
      : super(defaultTargetPlatform == TargetPlatform.iOS
            ? DynamicLibrary.process()
            : DynamicLibrary.open("libjuce_lib.so"));
}
