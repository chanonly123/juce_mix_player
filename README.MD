## Mixer audio player using JUCE for Android/iOS

### Features
- Play multiple local audio with track composition
- Record audio simultaneously
- Available device lising and selection
- Platform independent code
- Supported sample rate is 48000

### Demo
[![](https://markdown-videos-api.jorgenkh.no/youtube/3ZiBKfGdLjs)](https://youtu.be/3ZiBKfGdLjs)

### Requirements
- Android Studio (for Android)
- JUCE directory in `/Users/<username>/JUCE/` or `C:\JUCE\` (JUCE VERSION 8.0.5 [download](https://juce.com/download/))
- Xcode `xcodebuild -version` (for iOS)
- Cocoapods (`brew install cocoapods`). `pod --version` should work. (for iOS)
- Commands `flutter` and `dart` (to generate cpp to dart bindings)

### Regenerate Projucer generated projects
- open `juce_lib/juce_lib.jucer` using Projucer and hit (command+P to regenerate)
or
- run `sh setup_clean_projects.sh` or `win_setup_clean_projects.bat` (needs work)

### build flutter project
- inside `juce_mix_player_package` run `dart run ffigen`
- run flutter project normally

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
- Check `/flutter_app`

### License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Contact
- Email: chan.only.123@gmail.com
- Turorials [Bringing JUCE to Flutter](https://medium.com/@chan.only.123/bringing-juce-to-flutter-a-walk-in-the-park-part-1-flutter-ios-569dee72d7e8)