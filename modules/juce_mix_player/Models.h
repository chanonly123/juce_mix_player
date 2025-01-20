#pragma once

#include "nlohmann/json.hpp"

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
                                                enabled)

    bool operator==(const MixerTrack& other) const {
        return
        id_ == other.id_
        && path == other.path
        && volume == other.volume
        && offset == other.offset
        && fromTime == other.fromTime
        && duration == other.duration
        && repeat == other.repeat
        && repeatInterval == other.repeatInterval
        && enabled == other.enabled;
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
                                                outputDuration)

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

    /// Throws std::string
    static void isValid(MixerData& mixerData);

    /// Returns total duration in seconds. Requires reader for each track.
    static float getTotalDuration(MixerData& mixerData);
};
