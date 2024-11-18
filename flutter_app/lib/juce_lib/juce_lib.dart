import 'dart:ffi';

import 'package:flutter/foundation.dart';
import 'package:flutter_app/juce_lib/juce_lib_gen.dart';

class JuceLib extends JuceLibGen {
  JuceLib()
      : super(defaultTargetPlatform == TargetPlatform.iOS
            ? DynamicLibrary.process()
            : DynamicLibrary.open("libjuce_lib.so"));
}
