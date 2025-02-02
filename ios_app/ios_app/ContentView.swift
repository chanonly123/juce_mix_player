import SwiftUI
import AVKit

struct ContentView: View {

    var body: some View {
//        NavigationLink {
        PlayerPage(record: true, play: true)
//        } label: {
//            Text("ShowPlayer")
//        }
    }
}

struct PlayerPage: View {

    @State private var player: JuceMixPlayer
    @State private var progress: Float = 0
    @State private var sliderEditing: Bool = false
    @State private var playerState: JuceMixPlayerState = .IDLE
    @State private var recState: JuceMixPlayerRecState = .IDLE
    @State private var recProgress: Float = 0
    @State private var recLevel: Float = 0
    @State private var recLevelMax: Float = 0

    @State private var deviceList = MixerDeviceList(devices: [])

    var recordFile: String {
        FileManager.default.temporaryDirectory.appending(path: "/rec.wav").path()
    }

    init(record: Bool, play: Bool) {
        _player = State(wrappedValue: JuceMixPlayer(record: record, play: play))
    }

    var body: some View {
        VStack {
            Spacer()

            Text("\(playerState.rawValue)")

            Text("\(progress * player.getDuration()) / \(player.getDuration())")

            Slider(value: $progress) { editing in
                sliderEditing = editing
                if !editing {
                    player.seek(value: progress)
                }
            }

            VStack {
                HStack {
                    Button(playerState == .PLAYING ? "pause" : "play") {
                        player.togglePlayPause()
                    }
                }

                Button("Set recorded file") {
                    player.setFile(recordFile)
                }

                Button("Set file") {
                    let path = Bundle.main.path(forResource: "music_small", ofType: "wav")!
                    player.setFile(path)
                }

                Button("Set multiple") {
                    let path1 = Bundle.main.path(forResource: "music_big", ofType: "mp3")!
                    let path2 = Bundle.main.path(forResource: "music_small", ofType: "wav")!
                    player.setData(
                        MixerData(
                            tracks: [
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

            Spacer()

            Text("\(recState)") + Text("Time: \(recProgress)")
            Text("Levels: \(recLevel), Max: \(recLevelMax)")

            Button("Prepare Recorder") {
                player.prepareRecorder(file: recordFile);
                player.setSettings(MixerSettings(progressUpdateInterval: 0.05, sampleRate: 48000))
            }
            HStack {
                Button(recState == .RECORDING ? "Stop Rec" : "Start Rec") {
                    if recState == .RECORDING {
                        player.stopRecorder()
                    } else {
                        player.startRecorder();
                    }
                }

                Button(recState == .RECORDING ? "Pause & Stop rec" : "Play & Start rec") {
                    if recState == .RECORDING {
                        player.stopRecorder()
                        player.stop()
                    } else {
                        player.startRecorder();
                        player.play()
                    }
                }
            }

            Spacer()

            Menu("DEVICES: " + "\(deviceList.devices.count)") {
                ForEach(deviceList.devices.indices, id: \.self) { i in
                    let dev = deviceList.devices[i]
                    Button(getName(dev)) {
                        deviceList.devices.forEach({
                            if $0.isInput == dev.isInput {
                                $0.isSelected = false
                            }
                        })
                        dev.isSelected = true
                        player.setUpdatedDevices(devices: deviceList)
                    }
                }
            }

            Spacer()
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
                if state == .COMPLETED {
                    player.stopRecorder()
                }
            }
            player.setErrorHandler { error in
                print("error: \(error)")
            }

            player.setRecProgressHandler { progress in
                recProgress = progress
            }
            player.setRecStateUpdateHandler { state in
                recState = state
            }
            player.setRecErrorHandler { error in
                print("rec error: \(error)")
            }
            player.setRecLevelHandler { level in
                recLevel = level
                if recLevel > recLevelMax {
                    recLevelMax = recLevel
                }
            }
            player.setDeviceUpdateHandler { devices in
                deviceList = devices
            }
        }
    }

    func getName(_ dev: MixerDevice) -> String {
        dev.name + (dev.isInput ? " (input)" : " (output)") + (dev.isSelected ? " ✅" : "")
    }
}

#Preview {
    PlayerPage(record: false, play: false)
}
