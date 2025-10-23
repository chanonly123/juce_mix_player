#pragma once

#include "Models.h"

bool setAudioSessionPlay();
bool setAudioSessionRecord(MixerSettings& settings);

juce::AudioDeviceManager* _sharedDeviceManager;
juce::AudioDeviceManager* sharedDeviceManager();
