## Demo Flutter project juce audio player for Android/iOS

### Demo
[![](https://markdown-videos-api.jorgenkh.no/youtube/3ZiBKfGdLjs)](https://youtu.be/3ZiBKfGdLjs)

### Requirements
- Android Studio (for Android)
- JUCE directory in `/Users/<username>/JUCE/` or `C:\JUCE\` (JUCE VERSION 8.0.2 [download](https://juce.com/download/))
- Xcode `xcodebuild -version` (for iOS)
- Cocoapods (`brew install cocoapods`). `pod --version` should work. (for iOS)
- `flutter` and `dart` (to generate cpp to dart bindings)
- supported sample rate is 48000. (Variable rate is coming soon) 

### Regenerate Projucer generated projects
- open `juce_lib/juce_lib.jucer` using Projucer and hit (command+P to regenerate)
or
- run `sh setup_clean_projects.sh` or `win_setup_clean_projects.bat`

### Development
- To develop the native code use `ios_app/ios_app.xcodeproj`, (to debug native code) (not possible for windows for now)

### build flutter project
- run `sh android_juce_lib_build.sh` or `win_android_build.bat` or `ios_juce_lib_build.sh`. This will build juce native library and copy output binary to flutter project
- run flutter project normally
or
- use vscode launch tasks `flutter_app_android` and `flutter_app_ios`

### Usage
- The player takes json string input
```
{
    "tracks": [
        {
            "id_": "music",
            "path": "/data/user/0/com.example.flutter_app/app_flutter/assets/media/music_big.mp3"
        },
        {
            "id_": "met_1",
            "path": "/data/user/0/com.example.flutter_app/app_flutter/assets/media/met_h.wav",
            "offset": 0.0,
            "volume": 0.1,
            "repeat": true,
            "repeatInterval": 2.0
        },
        {
            "id_": "met_2",
            "path": "/data/user/0/com.example.flutter_app/app_flutter/assets/media/met_l.wav",
            "offset": 0.5,
            "volume": 0.1,
            "repeat": true,
            "repeatInterval": 2.0
        },
        {
            "id_": "met_3",
            "path": "/data/user/0/com.example.flutter_app/app_flutter/assets/media/met_l.wav",
            "offset": 1.0,
            "volume": 0.1,
            "repeat": true,
            "repeatInterval": 2.0
        },
        {
            "id_": "met_4",
            "path": "/data/user/0/com.example.flutter_app/app_flutter/assets/media/met_l.wav",
            "offset": 1.5,
            "volume": 0.1,
            "repeat": true,
            "repeatInterval": 2.0
        }
    ]
}
```
- Check `/flutter_app` or `/ios_app` for details

### License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Contact
- email: chan.only.123@gmail.com