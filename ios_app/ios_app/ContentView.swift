//
//  ContentView.swift
//  ios_app
//
//  Created by Chandan on 27/09/24.
//

import SwiftUI
import AVKit

typealias StateUpdateCallback = @convention(c) (UnsafePointer<Int8>?) -> Void
typealias OnProgresCallback = @convention(c) (Float) -> Void

func onStateUpdate(state: UnsafePointer<Int8>!) {
    let str1 = String(cString: state)
    print("onStateUpdate: \(str1)")
}

func onProgress(progress: Float) {
    print("onProgress: \(progress)")
}

func onError(error: UnsafePointer<Int8>!) {
    let err = String(cString: error)
    print("onError: \(err)")
}

struct ContentView: View {

    @State var player: UnsafeMutableRawPointer!

    var body: some View {
        VStack(spacing: 24) {

            Button("Init first") {
                executeAsync {
                    player = JuceMixPlayer_init()
                    JuceMixPlayer_onStateUpdate(player, onStateUpdate as StateUpdateCallback)
                    JuceMixPlayer_onProgress(player, onProgress as OnProgresCallback)
                }
            }

            Button("Load smaller") {
                executeAsync {
                    resetPlayer(small: true)
                }
            }

            Button("Load big") {
                executeAsync {
                    resetPlayer(small: false)
                }
            }

            Button("play") {
                executeAsync {
                    JuceMixPlayer_play(player)
                }
            }

            Button("Pause") {
                executeAsync {
                    JuceMixPlayer_pause(player)
                }
            }

            Button("Stop") {

            }

            Button("Destroy") {
                JuceMixPlayer_deinit(player)
            }
        }
        .padding()
        .onAppear {
            do {
                try AVAudioSession.sharedInstance().setCategory(.playback)
                try AVAudioSession.sharedInstance().setActive(true)
            } catch let err {
                print("\(err)")
            }
        }
    }

    func resetPlayer(small: Bool) {
        let path1 = Bundle.main.path(forResource: "music_small", ofType: "wav")!
        let path2 = Bundle.main.path(forResource: "music_big", ofType: "mp3")!
        let json = """
{
    "output": "/Users/apple/Downloads/out.wav",
    "outputDuration": 0,
    "tracks": [
        {
            "id_": "vocal",
            "path": "\(small ? path1 : path2)",
            "volume": 1.0,
            "offset": 2,
            "fromTime": 10,
            "duration" : 0,
            "enabled": true
        },
        {
            "id_": "music",
            "path": "/Users/apple/Documents/Melodyze/melodyze-juce-example-ios/sample_ios_app/JuceKitTests/music.mp3",
            "volume": 1.0,
            "offset": 0,
            "enabled": false
        },
        {
            "id_": "meth",
            "path": "/Users/apple/Documents/Melodyze/melodyze-juce-example-ios/sample_ios_app/JuceKitTests/met_h.wav",
            "volume": 1.0,
            "offset": 0,
            "repeat": true,
            "repeatInterval": 2.0,
            "enabled": false
        }
    ]
}
"""
        JuceMixPlayer_set(player, json.cString(using: .utf8))
    }
}

//#Preview {
//    ContentView()
//}
