## Demo Flutter project juce audio player for Android/iOS

### Requirements
- Macos
- JUCE directory in `/Users/<username>/JUCE/` (JUCE VERSION 8.0.2 [download](https://juce.com/download/))
- Xcode `xcodebuild -version`
- Cocoapods (`brew install cocoapods`). `pod --version` should work.
- Gradle (`brew install gradle`) `gradle --version` should work.

### Regenerate Projucer generated projects
- open `juce_lib/juce_lib.jucer` using Projucer and hit (command+P to regenerate)
or
- run `sh setup_clean_projects.sh`

### Development
- To develop the native code use `ios_app/ios_app.xcodeproj`, (to debug native code)

### build flutter project
- run `sh android_juce_lib_build.sh` or `ios_juce_lib_build.sh`. This will build juce library and copy binary to flutter project
- run flutter project normally
or
- use vscode launch tasks `flutter_app_android` and `flutter_app_ios`

### Known issues
- Player implementation is not proper. Need to work on the threads.

#### Contact
- email: chan.only.123@gmail.com