#pragma once

#include <JuceHeader.h>
#include <unordered_set>
#include "Logger.h"
#include "TaskQueue.h"
#include "Models.h"
#include "Models.h"
#include <iostream>

typedef void (*JuceMixPlayerOnProgress)(float);
typedef void (*JuceMixPlayerOnStateUpdate)(const char*);

enum JuceMixPlayerState
{
    IDLE, PLAYING, PAUSED, STOPPED, READY, LOADING, ERROR
};

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

    JuceMixPlayerState state = IDLE;

    JuceMixPlayerOnProgress onProgressCallback = nullptr;
    JuceMixPlayerOnStateUpdate onStateUpdateCallback = nullptr;

    bool isPlaying = false;

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

    void _prepare();

    void _loadAudioBlock(int block);

    void _onProgressNotify(float progress);

    void _onStateUpdateNotify(std::string state);

public:
    JuceMixPlayer();

    ~JuceMixPlayer();

    void play();

    void pause();

    void stop();

    void togglePlayPause();

    void set(const char* json);

    void prepare();

    void onProgress(JuceMixPlayerOnProgress callback);

    void onStateUpdate(JuceMixPlayerOnStateUpdate callback);

    float getCurrentTime();

    /// juce::AudioSource
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override;

    void releaseResources() override;

    /// juce::Timer
    void timerCallback() override;
};

class JuceMixPlayerRef {
public:
    std::shared_ptr<JuceMixPlayer> ptr;

    JuceMixPlayerRef();
};
