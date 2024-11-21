## Demo Flutter project juce audio player for Android/iOS

### Requirements
- JUCE directory in `/Users/<username>/JUCE/` (JUCE VERSION 8.0.2 [download](https://juce.com/download/))
- Xcode
- Cocoapods (brew install cocoapods)
- Gradle (brew install gradle)

### Regenerate Projucer generated projects
- open `juce_lib/juce_lib.jucer` using Projucer and hit (command+P to regenerate)

### Develop use Xcode
- To develop the native code use `ios_app/ios_app.xcodeproj`

### To build flutter project
- `cd flutter_app` then `flutter pub get` then `dart run ffigen`
- back to root directory and `sh android_juce_lib_build.sh` or `ios_juce_lib_build.sh`. This will build juce library and copy binary to flutter project
- set some path to audio file in `flutter_app/lib/main.dart`.
- run flutter project normally

### Known issues
- Flutter android project is not working (something wrong with main thread init)

#### Contact
- email: chan.only.123@gmail.com