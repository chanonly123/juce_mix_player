import Foundation

typealias StringUpdateCallback = @convention(c) (UnsafeMutableRawPointer?, UnsafePointer<Int8>?) -> Void
typealias FloatCallback = @convention(c) (UnsafeMutableRawPointer?, Float) -> Void

func onStateUpdate(player: UnsafeMutableRawPointer!, state: UnsafePointer<Int8>!) {
    let str = String(cString: state)
    closuresStateUpdate[player]?(JuceMixPlayerState(rawValue: str)!)
}

func onProgress(player: UnsafeMutableRawPointer!, progress: Float) {
    closuresProgress[player]?(progress)
}

func onError(player: UnsafeMutableRawPointer!, error: UnsafePointer<Int8>!) {
    let err = String(cString: error)
    closuresError[player]?(err)
}

extension Encodable {
    func jsonString() throws -> String {
        let data = try JSONEncoder().encode(self)
        return String(data: data, encoding: .utf8)!
    }
}

enum JuceMixPlayerState: String {
    case IDLE, READY, PLAYING, PAUSED, STOPPED, ERROR, COMPLETED
}

private var closuresProgress: [UnsafeMutableRawPointer: (Float)->Void] = [:]
private var closuresStateUpdate: [UnsafeMutableRawPointer: (JuceMixPlayerState)->Void] = [:]
private var closuresError: [UnsafeMutableRawPointer: (String)->Void] = [:]

class JuceMixPlayer {
    private lazy var player: UnsafeMutableRawPointer = JuceMixPlayer_init()

    init() {
        print("swift JuceMixPlayer")
    }

    func setFile(_ file: String) {
        let data = MixerData(
            tracks: [
                MixerTrack(path: file)
            ]
        )
        let str = (try? data.jsonString()) ?? ""
        JuceMixPlayer_set(player, str.cString(using: .utf8))
    }

    func play() {
        JuceMixPlayer_play(player)
    }

    func pause() {
        JuceMixPlayer_pause(player)
    }

    func togglePlayPause() {
        if JuceMixPlayer_isPlaying(player) == 0 {
            play()
        } else {
            pause()
        }
    }

    func stop() {
        JuceMixPlayer_stop(player)
    }

    func isPlaying() -> Bool {
        JuceMixPlayer_isPlaying(player) == 1
    }

    func getDuration() -> Float {
        return JuceMixPlayer_getDuration(player)
    }

    func setProgressHandler(_ handler: @escaping (Float)->Void) {
        closuresProgress[player] = handler
        JuceMixPlayer_onProgress(player, onProgress as FloatCallback)
    }

    func setStateUpdateHandler(_ handler: @escaping (JuceMixPlayerState)->Void) {
        closuresStateUpdate[player] = handler
        JuceMixPlayer_onStateUpdate(player, onStateUpdate as StringUpdateCallback)
    }

    func setErrorHandler(handler: @escaping (String)->Void) {
        closuresError[player] = handler
        JuceMixPlayer_onError(player, onError as StringUpdateCallback)
    }

    func seek(value: Float) {
        JuceMixPlayer_seek(player, value)
    }

    deinit {
        print("swift ~JuceMixPlayer")
        closuresError[player] = nil
        closuresStateUpdate[player] = nil
        closuresProgress[player] = nil
        JuceMixPlayer_deinit(player)
    }
}

struct MixerData: Codable {

    let tracks: [MixerTrack]?
    let output: String?
    let outputDuration: Int?

    init(tracks: [MixerTrack]?) {
        self.tracks = tracks
        self.output = nil
        self.outputDuration = nil
    }

    enum CodingKeys: String, CodingKey {
        case tracks = "tracks"
        case output = "output"
        case outputDuration = "outputDuration"
    }
}

struct MixerTrack: Codable {

    let duration: Int?
    let volume: Double?
    let fromTime: Int?
    let enabled: Bool?
    let path: String?
    let offset: Int?
    let id: String?

    init(path: String) {
        self.path = path
        self.enabled = true
        self.offset = nil
        self.id = nil
        self.fromTime = nil
        self.volume = nil
        self.duration = nil
    }

    enum CodingKeys: String, CodingKey {
        case duration = "duration"
        case volume = "volume"
        case fromTime = "fromTime"
        case enabled = "enabled"
        case path = "path"
        case offset = "offset"
        case id = "id_"
    }
}
