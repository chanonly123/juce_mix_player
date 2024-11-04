//
//  ContentView.swift
//  juce_player_test
//
//  Created by Chandan on 27/09/24.
//

import SwiftUI
import AVKit

struct ContentView: View {

    @State var player = native_init("JuceMixPlayer")
    @State var item = native_init("JuceMixItem")

    var body: some View {
        VStack(spacing: 24) {
            Button("Open file") {
                let path = Bundle.main.path(forResource: "music", ofType: "wav")!
                native_call(item, "setPath", ["path": path].toJsonString)
                native_call(player, "addItem", ["item": item].toJsonString)
            }

            Button("play") {
                native_call(player, "play", nil)
            }

            Button("Pause") {
                native_call(player, "pause", nil)
            }

            Button("Stop") {
                native_call(player, "stop", nil)
            }

            Button("Destroy") {
                native_call(player, "destroy", nil)
                native_call(item, "destroy", nil)
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
}

#Preview {
    ContentView()
}
