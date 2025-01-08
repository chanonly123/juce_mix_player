import SwiftUI
import AVKit

struct ContentView: View {

    var body: some View {
        NavigationLink {
            PlayerPage()
        } label: {
            Text("ShowPlayer")
        }
    }
}

struct PlayerPage: View {

    @State private var player = JuceMixPlayer()
    @State private var progress: Float = 0
    @State private var sliderEditing: Bool = false
    @State private var playerState: JuceMixPlayerState = .IDLE

    var body: some View {
        VStack {
            Text("\(playerState.rawValue)")

            Text("\(progress * player.getDuration()) / \(player.getDuration())")

            Slider(value: $progress) { editing in
                sliderEditing = editing
                if !editing {
                    player.seek(value: progress)
                }
            }

            HStack {
                Button(playerState == .PLAYING ? "pause" : "play") {
                    player.togglePlayPause()
                }
                Button("Set file") {
                    let path = Bundle.main.path(forResource: "music_big", ofType: "mp3")!
//                    let path = Bundle.main.path(forResource: "music_small", ofType: "wav")!
                    player.setFile(path)
                }
            }
        }
        .padding()
        .buttonStyle(.bordered)
        .monospaced()
        .onAppear {
            player.setProgressHandler { progress in
                if !sliderEditing {
                    withAnimation {
                        self.progress = progress
                    }
                }
            }
            player.setStateUpdateHandler { state in
                playerState = state
            }
            player.setErrorHandler { error in
                print("error: \(error)")
            }
        }
    }
}

//#Preview {
//    ContentView()
//}
