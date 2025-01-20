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
    private lazy var ptr: UnsafeMutableRawPointer = JuceMixPlayer_init(_record ? 1 : 0, _play ? 1 : 0)

    private let _record, _play: Bool

    init(record: Bool, play: Bool) {
        print("swift JuceMixPlayer")
        self._record = record
        self._play = play
    }

    func setFile(_ file: String) {
        let data = MixerData(
            tracks: [
                MixerTrack(id: "0", path: file)
            ]
        )
        let str = (try? data.jsonString()) ?? ""
        JuceMixPlayer_set(ptr, str.cString(using: .utf8))
    }

    func setData(_ data: MixerData) {
        let str = (try? data.jsonString()) ?? ""
        JuceMixPlayer_set(ptr, str.cString(using: .utf8))
    }

    func play() {
        JuceMixPlayer_play(ptr)
    }

    func pause() {
        JuceMixPlayer_pause(ptr)
    }

    func togglePlayPause() {
        if JuceMixPlayer_isPlaying(ptr) == 0 {
            play()
        } else {
            pause()
        }
    }

    func stop() {
        JuceMixPlayer_stop(ptr)
    }

    func isPlaying() -> Bool {
        JuceMixPlayer_isPlaying(ptr) == 1
    }

    func getDuration() -> Float {
        return JuceMixPlayer_getDuration(ptr)
    }

    func setProgressHandler(_ handler: @escaping (Float)->Void) {
        closuresProgress[ptr] = handler
        JuceMixPlayer_onProgress(ptr, onProgress as FloatCallback)
    }

    func setStateUpdateHandler(_ handler: @escaping (JuceMixPlayerState)->Void) {
        closuresStateUpdate[ptr] = handler
        JuceMixPlayer_onStateUpdate(ptr, onStateUpdate as StringUpdateCallback)
    }

    func setErrorHandler(handler: @escaping (String)->Void) {
        closuresError[ptr] = handler
        JuceMixPlayer_onError(ptr, onError as StringUpdateCallback)
    }

    func seek(value: Float) {
        JuceMixPlayer_seek(ptr, value)
    }

    func startRecorder(file: String) {
        JuceMixPlayer_startRecorder(ptr, file.cString(using: .utf8))
    }

    func stopRecorder() {
        JuceMixPlayer_stopRecorder(ptr)
    }

    deinit {
        print("swift ~JuceMixPlayer")
        closuresError[ptr] = nil
        closuresStateUpdate[ptr] = nil
        closuresProgress[ptr] = nil
        JuceMixPlayer_deinit(ptr)
    }
}

struct MixerData: Codable {

    let tracks: [MixerTrack]?
    let outputDuration: Double?

    init(tracks: [MixerTrack]?, outputDuration: Double? = nil) {
        self.tracks = tracks
        self.outputDuration = outputDuration
    }

    enum CodingKeys: String, CodingKey {
        case tracks = "tracks"
        case outputDuration = "outputDuration"
    }
}

struct MixerTrack: Codable {

    let duration: Double?
    let volume: Double?
    let fromTime: Double?
    let enabled: Bool?
    let path: String?
    let offset: Double?
    let id: String?
    let repeat_: Bool?
    let repeatInterval: Double?

    init(
        id: String,
        path: String,
        offset: Double? = nil,
        fromTime: Double? = nil,
        duration: Double? = nil,
        `repeat`: Bool? = nil,
        repeatInterval: Double? = nil,
        volume: Double? = nil,
        enabled: Bool? = nil
    ) {
        self.path = path
        self.enabled = enabled
        self.offset = offset
        self.id = id
        self.fromTime = fromTime
        self.volume = volume
        self.duration = duration
        self.repeat_ = `repeat`
        self.repeatInterval = repeatInterval
    }

    enum CodingKeys: String, CodingKey {
        case duration = "duration"
        case volume = "volume"
        case fromTime = "fromTime"
        case enabled = "enabled"
        case path = "path"
        case offset = "offset"
        case id = "id_"
        case repeat_ = "repeat"
        case repeatInterval = "repeatInterval"
    }
}
