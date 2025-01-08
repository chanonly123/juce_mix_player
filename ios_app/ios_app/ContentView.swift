import SwiftUI
import AVKit

struct ContentView: View {

    @State private var player = JuceMixPlayer()
    @State private var progress: Float = 0
    @State private var sliderEditing: Bool = false

    var body: some View {
        VStack {
            Slider(value: $progress) { editing in
                sliderEditing = editing
                if !editing {
                    player.seek(value: progress)
                }
            }
            HStack {
                Button(player.isPlaying() ? "pause" : "play") {
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
        .onAppear {
            player.setProgressHandler { progress in
                if !sliderEditing {
                    withAnimation {
                        self.progress = progress
                    }
                }
            }
        }
    }
}

//#Preview {
//    ContentView()
//}
