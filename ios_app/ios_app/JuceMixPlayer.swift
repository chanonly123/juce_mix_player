import Foundation

extension Encodable {
    func jsonString() throws -> String {
        let data = try JSONEncoder().encode(self)
        return String(data: data, encoding: .utf8)!
    }
}

enum JuceMixPlayerState: String {
    case IDLE, READY, PLAYING, PAUSED, STOPPED, ERROR, COMPLETED
}

enum JuceMixPlayerRecState: String {
    case IDLE, READY, RECORDING, STOPPED, ERROR
}

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

private var closuresProgress: [UnsafeMutableRawPointer: (Float)->Void] = [:]
private var closuresStateUpdate: [UnsafeMutableRawPointer: (JuceMixPlayerState)->Void] = [:]
private var closuresError: [UnsafeMutableRawPointer: (String)->Void] = [:]

func onRecStateUpdate(player: UnsafeMutableRawPointer!, state: UnsafePointer<Int8>!) {
    let str = String(cString: state)
    closuresRecStateUpdate[player]?(JuceMixPlayerRecState(rawValue: str)!)
}

func onRecProgress(player: UnsafeMutableRawPointer!, progress: Float) {
    closuresRecProgress[player]?(progress)
}

func onRecError(player: UnsafeMutableRawPointer!, error: UnsafePointer<Int8>!) {
    let err = String(cString: error)
    closuresRecError[player]?(err)
}

func onRecLevel(player: UnsafeMutableRawPointer!, level: Float) {
    closuresRecLevel[player]?(level)
}

private var closuresRecProgress: [UnsafeMutableRawPointer: (Float)->Void] = [:]
private var closuresRecStateUpdate: [UnsafeMutableRawPointer: (JuceMixPlayerRecState)->Void] = [:]
private var closuresRecError: [UnsafeMutableRawPointer: (String)->Void] = [:]
private var closuresRecLevel: [UnsafeMutableRawPointer: (Float)->Void] = [:]

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

    func setSettings(_ settings: MixerSettings) {
        let str = (try? settings.jsonString()) ?? ""
        JuceMixPlayer_setSettings(ptr, str.cString(using: .utf8))
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

    // MARK: Recorder

    func setRecProgressHandler(_ handler: @escaping (Float)->Void) {
        closuresRecProgress[ptr] = handler
        JuceMixPlayer_onRecProgress(ptr, onRecProgress as FloatCallback)
    }

    func setRecStateUpdateHandler(_ handler: @escaping (JuceMixPlayerRecState)->Void) {
        closuresRecStateUpdate[ptr] = handler
        JuceMixPlayer_onRecStateUpdate(ptr, onRecStateUpdate as StringUpdateCallback)
    }

    func setRecErrorHandler(handler: @escaping (String)->Void) {
        closuresRecError[ptr] = handler
        JuceMixPlayer_onRecError(ptr, onRecError as StringUpdateCallback)
    }

    func setRecLevelHandler(_ handler: @escaping (Float)->Void) {
        closuresRecLevel[ptr] = handler
        JuceMixPlayer_onRecLevel(ptr, onRecLevel as FloatCallback)
    }

    func prepareRecorder(file: String) {
        JuceMixPlayer_prepareRecorder(ptr, file.cString(using: .utf8))
    }

    func startRecorder() {
        JuceMixPlayer_startRecorder(ptr)
    }

    func stopRecorder() {
        JuceMixPlayer_stopRecorder(ptr)
    }

    deinit {
        print("swift ~JuceMixPlayer")
        closuresError[ptr] = nil
        closuresStateUpdate[ptr] = nil
        closuresProgress[ptr] = nil
        closuresRecError[ptr] = nil
        closuresRecStateUpdate[ptr] = nil
        closuresRecProgress[ptr] = nil
        closuresRecLevel[ptr] = nil
        JuceMixPlayer_deinit(ptr)
    }
}

struct MixerSettings: Codable {
    let progressUpdateInterval: Double?
    let sampleRate: Int?
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
