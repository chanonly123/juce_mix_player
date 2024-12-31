//
//  ContentView.swift
//  ios_app
//
//  Created by Chandan on 27/09/24.
//

import SwiftUI
import AVKit

func swiftListener(arg1: UnsafePointer<CChar>, arg2: UnsafePointer<CChar>, value: Float) {
    let str1 = String(cString: arg1)
    let str2 = String(cString: arg2)
    print("Swift Listener called with: \(str1), \(str2), \(value)")
}

struct ContentView: View {

    @State var player: UnsafeMutableRawPointer!
    @State var item: UnsafeMutableRawPointer!

    var body: some View {
        VStack(spacing: 24) {

            Button("Init first") {
                executeAsync {
                    player = JuceMixPlayer_init()
                    item = JuceMixItem_init()
                }
            }

            Button("Open file") {
                executeAsync {
                    resetPlayer()
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
                JuceMixItem_deinit(item)
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
            testParseModel()
        }
    }

    func resetPlayer() {
        let path = Bundle.main.path(forResource: "music", ofType: "mp3")!
        let json = """
{
    "output": "/Users/apple/Downloads/out.wav",
    "outputDuration": 102.88800048828125,
    "tracks": [
        {
            "id_": "vocal",
            "path": "\(path)",
            "volume": 1.0,
            "offset": 2,
            "fromTime": 10,
            "duration" : ,
            "enable": true
        },
        {
            "id_": "music",
            "path": "/Users/apple/Documents/Melodyze/melodyze-juce-example-ios/sample_ios_app/JuceKitTests/music.mp3",
            "volume": 1.0,
            "offset": 0,
            "enable": false
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


        JuceMixPlayer_reset(player, item)
    }
}

//#Preview {
//    ContentView()
//}
