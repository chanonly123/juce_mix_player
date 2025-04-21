#pragma once

#include "nlohmann/json.hpp"

typedef void (*JuceMixPlayerCallbackFloat)(void*, float);
typedef void (*JuceMixPlayerCallbackString)(void*, const char*);

enum class JuceMixPlayerState {
    IDLE, READY, PLAYING, PAUSED, STOPPED, ERROR, COMPLETED
};

enum class JuceMixPlayerRecState {
    IDLE, READY, RECORDING, STOPPED, ERROR
};

NLOHMANN_JSON_SERIALIZE_ENUM(JuceMixPlayerRecState,{
    {JuceMixPlayerRecState::IDLE, "IDLE"},
    {JuceMixPlayerRecState::READY, "READY"},
    {JuceMixPlayerRecState::RECORDING, "RECORDING"},
    {JuceMixPlayerRecState::STOPPED, "STOPPED"},
    {JuceMixPlayerRecState::ERROR, "ERROR"},
});

std::string JuceMixPlayerRecState_toString(JuceMixPlayerRecState state);

NLOHMANN_JSON_SERIALIZE_ENUM(JuceMixPlayerState,{
    {JuceMixPlayerState::IDLE, "IDLE"},
    {JuceMixPlayerState::READY, "READY"},
    {JuceMixPlayerState::PLAYING, "PLAYING"},
    {JuceMixPlayerState::PAUSED, "PAUSED"},
    {JuceMixPlayerState::STOPPED, "STOPPED"},
    {JuceMixPlayerState::ERROR, "ERROR"},
    {JuceMixPlayerState::COMPLETED, "COMPLETED"},
});

std::string JuceMixPlayerState_toString(JuceMixPlayerState state);

struct MixerDevice {
    std::string name = "";
    bool isInput = false;
    bool isSelected = false;
    
    // New fields for device details
    std::vector<std::string> inputChannelNames;
    std::vector<std::string> outputChannelNames;
    double currentSampleRate = 0.0;
    std::vector<double> availableSampleRates;
    std::string deviceType;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MixerDevice,
                                                name,
                                                isInput,
                                                isSelected,
                                                inputChannelNames,
                                                outputChannelNames,
                                                currentSampleRate,
                                                availableSampleRates,
                                                deviceType);

    bool operator==(const MixerDevice& other) const {
        return name == other.name &&
               isInput == other.isInput &&
               isSelected == other.isSelected &&
               inputChannelNames == other.inputChannelNames &&
               outputChannelNames == other.outputChannelNames &&
               currentSampleRate == other.currentSampleRate &&
               availableSampleRates == other.availableSampleRates &&
               deviceType == other.deviceType;
    }
};

struct MixerDeviceList {

    std::vector<MixerDevice> devices = {};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MixerDeviceList,
                                                devices);

    bool operator==(const MixerDeviceList& other) const {
        return
        devices == other.devices;
    }

    static MixerDeviceList decode(std::string str) {
        nlohmann::json jsonPerson = nlohmann::json::parse(str);
        return jsonPerson.get<MixerDeviceList>();
    }
};

struct MixerSettings {
    // seconds
    float progressUpdateInterval = 0.05;
    // player and recorder sample rate
    int sampleRate = 48000;
    // stop record on playback ends
    bool stopRecOnPlaybackComplete = false;
    // audio Playback loop
    bool loop = false;
    // audio playback during rec
    bool recBgPlayback = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MixerSettings,
                                                progressUpdateInterval,
                                                sampleRate,
                                                loop,
                                                recBgPlayback,
                                                stopRecOnPlaybackComplete);
};

struct MixerTrack {

    std::string id_ = "";

    // absolute path of the file
    std::string path = "";

    // volume of the track
    float volume = 1;

    // delay in seconds
    float offset = 0;

    // 0 -> copy from begining (seconds)
    float fromTime = 0;

    // 0 -> full duration (seconds)
    float duration = 0;

    // repeatedly inserts the audio
    bool repeat = false;

    // interval of the repeating audio
    float repeatInterval = 0;

    // default value `true`
    bool enabled = true;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MixerTrack,
                                                id_,
                                                path,
                                                volume,
                                                offset,
                                                fromTime,
                                                duration,
                                                repeat,
                                                repeatInterval,
                                                enabled);

    bool operator==(const MixerTrack& other) const {
        return
        id_ == other.id_
        && path == other.path
//        && volume == other.volume
//        && offset == other.offset
//        && fromTime == other.fromTime
//        && duration == other.duration
//        && enabled == other.enabled
//        && repeat == other.repeat
//        && repeatInterval == other.repeatInterval
        ;
    }

    std::shared_ptr<juce::AudioFormatReader> reader;
};

struct MixerData {

    // multiple track objects creates the result media
    std::vector<MixerTrack> tracks = {};

    std::string output = "";

    // strict output duration in seconds, else default value `0` means dynamic.
    float outputDuration = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MixerData,
                                                tracks,
                                                output,
                                                outputDuration);

    bool operator==(const MixerData& other) const {
        return
        outputDuration == other.outputDuration
        && output == other.output
        && tracks == other.tracks;
    }

};

class MixerModel {
public:
    static MixerData parse(const char* json);

    static MixerSettings parseSettings(const char* json);

    static void isValid(MixerData& mixerData);

    static void isValid(MixerSettings& settings);

    /// Returns total duration in seconds. Requires reader for each track.
    static float getTotalDuration(MixerData& mixerData);
};
