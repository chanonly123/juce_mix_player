#include "Models.h"

MixerData MixerModel::parse(const char *json) {
    nlohmann::json jsonPerson = nlohmann::json::parse(json);
    MixerData data = jsonPerson.get<MixerData>();
    isValid(data);
    return data;
}

void MixerModel::isValid(MixerData& mixerData) {
    std::unordered_set<std::string> set;
    for(MixerTrack& track: mixerData.tracks) {
        if (set.find(track.id_) != set.end()) {
            throw std::runtime_error("Duplicate id found");
        }
        if (track.duration < 0) {
            throw "`duration` < 0";
        }
        if (track.fromTime < 0) {
            throw "`fromTime` < 0";
        }
        if (track.offset < 0) {
            throw "`offset` < 0";
        }
        if (track.repeatInterval < 0) {
            throw "`repeatInterval` < 0";
        }
        set.insert(track.id_);
    }
}

float MixerModel::getTotalDuration(MixerData& mixerData) {
    float duration = mixerData.outputDuration;
    if (duration == 0 ) {
        for (MixerTrack& track: mixerData.tracks) {
            if (track.enabled && track.reader) {
                float dur = (track.duration == 0 ? (float)track.reader->lengthInSamples/track.reader->sampleRate : track.duration);
                float end = dur - track.fromTime + track.offset;
                if (end > duration) {
                    duration = end;
                }
            }
        }
    }
    return duration;
}
