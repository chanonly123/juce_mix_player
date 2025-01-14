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

            VStack {
                Button(playerState == .PLAYING ? "pause" : "play") {
                    player.togglePlayPause()
                }
                Button("Set file") {
                    let path = Bundle.main.path(forResource: "music", ofType: "mp3")!
//                    let path = Bundle.main.path(forResource: "music_big", ofType: "mp3")!
//                    let path = Bundle.main.path(forResource: "music_small", ofType: "wav")!
                    player.setFile(path)
                }

                Button("Set multiple") {
                    let path0 = Bundle.main.path(forResource: "music", ofType: "mp3")!
                    let path1 = Bundle.main.path(forResource: "music_big", ofType: "mp3")!
                    let path2 = Bundle.main.path(forResource: "music_small", ofType: "wav")!
                    player.setData(
                        MixerData(
                            tracks: [
                                MixerTrack(id: "0", path: path0, offset: 5, duration: 5, volume: 0.2, enabled: true),
                                MixerTrack(id: "1", path: path1, offset: 0, volume: 1, enabled: false),
                                MixerTrack(id: "2", path: path2, offset: 0, fromTime: 0, duration: 0, volume: 1, enabled: true)
                            ],
                            outputDuration: 150
                        )
                    )
                }

                Button("Set multiple metronome") {
                    let path1 = Bundle.main.path(forResource: "music_big", ofType: "mp3")!
                    let pathH = Bundle.main.path(forResource: "met_h", ofType: "wav")!
                    let pathL = Bundle.main.path(forResource: "met_l", ofType: "wav")!
                    player.setData(
                        MixerData(
                            tracks: [
                                MixerTrack(id: "music", path: path1),
                                MixerTrack(id: "met_1", path: pathH, offset: 0, repeat: true, repeatInterval: 2),
                                MixerTrack(id: "met_2", path: pathL, offset: 0.5, repeat: true, repeatInterval: 2),
                                MixerTrack(id: "met_3", path: pathL, offset: 1, repeat: true, repeatInterval: 2),
                                MixerTrack(id: "met_4", path: pathL, offset: 1.5, repeat: true, repeatInterval: 2)
                            ],
                            outputDuration: 150
                        )
                    )
                }
            }
        }
        .padding()
        .buttonStyle(.bordered)
        .monospaced()
        .onAppear {
            player.setProgressHandler { progress in
                if !sliderEditing {
                    self.progress = progress
                }
            }
            player.setStateUpdateHandler { state in
                playerState = state
                print("state: \(state)")
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
