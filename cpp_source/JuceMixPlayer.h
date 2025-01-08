#pragma once

#include <JuceHeader.h>
#include "nlohmann/json.hpp"
#include "Logger.h"
#include "TaskQueue.h"
#include "Models.h"
#include "Models.h"
#include <iostream>

typedef void (*JuceMixPlayerCallbackFloat)(void*, float);
typedef void (*JuceMixPlayerCallbackString)(void*, const char*);

enum JuceMixPlayerState
{
    IDLE, READY, PLAYING, PAUSED, STOPPED, ERROR
};

NLOHMANN_JSON_SERIALIZE_ENUM(JuceMixPlayerState,{
    {IDLE, "IDLE"},
    {READY, "READY"},
    {PLAYING, "PLAYING"},
    {PAUSED, "PAUSED"},
    {STOPPED, "STOPPED"},
    {ERROR, "ERROR"},
});

std::string JuceMixPlayerState_toString(JuceMixPlayerState state);
JuceMixPlayerState JuceMixPlayerState_make(std::string state);

class JuceMixPlayer : public juce::AudioSource,
private juce::Timer
{
private:
    TaskQueue taskQueue;
    int samplesPerBlockExpected = 0;
    float deviceSampleRate = 0;
    juce::AudioFormatManager formatManager;
    juce::LinearInterpolator interpolator[5];
    juce::AudioDeviceManager* deviceManager;
    juce::AudioSourcePlayer* player;

    JuceMixPlayerState currentState = IDLE;

    JuceMixPlayerCallbackFloat onProgressCallback = nullptr;
    JuceMixPlayerCallbackString onStateUpdateCallback = nullptr;
    JuceMixPlayerCallbackString onErrorCallback = nullptr;

    bool _isPlaying = false;

    juce::AudioBuffer<float> playBuffer;

    MixerData mixerData;

    float currentTime = 0;
    float lastSampleIndex = 0;

    std::unordered_set<int> loadingBlocks;
    std::unordered_set<int> loadedBlocks;
    const float blockDuration = 10; // second
    const float sampleRate = 48000;

    void _play();

    void _pause(bool stop);

    /// create reader for files
    void _prepareInternal();

    /// loads audio block for `blockDuration` and `block` number. Blocks are chunks of the audio file.
    void _loadAudioBlock(int block);

    void _onProgressNotify(float progress);

    void _onStateUpdateNotify(JuceMixPlayerState state);

    void _onErrorNotify(std::string error);

public:
    JuceMixPlayer();

    ~JuceMixPlayer();

    void dispose();

    void play();

    void pause();

    void stop();

    void togglePlayPause();

    void setJson(const char* json);

    void prepare();

    /// value range 0 to 1
    void seek(float value);

    void onProgress(JuceMixPlayerCallbackFloat callback);

    void onStateUpdate(JuceMixPlayerCallbackString callback);

    void onError(JuceMixPlayerCallbackString callback);

    // get curent time in seconds
    float getCurrentTime();

    // get total duration in seconds
    float getDuration();

    int isPlaying();

    std::string getCurrentState();

    /// juce::AudioSource
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override;

    void releaseResources() override;

    /// juce::Timer
    void timerCallback() override;
};
