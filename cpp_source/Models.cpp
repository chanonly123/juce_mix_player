#include "Models.h"

MixerData MixerModel::parse(const char *json) {
    nlohmann::json jsonPerson = nlohmann::json::parse(json);
    MixerData data = jsonPerson.get<MixerData>();
    return data;
}
