#pragma once

#include <unordered_set>
#include "Logger.h"
#include "TaskQueue.h"
#include "Models.h"
#include "JuceMixItem.h"
#include "Models.h"
#include <JuceHeader.h>
#include <iostream>

class JuceMixPlayer : public juce::AudioSource,
                      private juce::Timer
{
private:
    TaskQueue taskQueue;
    std::vector<JuceMixItem*> tracks;
    int samplesPerBlockExpected = 0;
    int deviceSampleRate = 0;
    juce::AudioFormatManager formatManager;
    juce::LinearInterpolator interpolator[5];
    juce::AudioDeviceManager* deviceManager;
    juce::AudioSourcePlayer* player;
    JuceMixPlayerState state = IDLE;
    ListenerFunction listener = nullptr;
    bool isPlaying = false;

    juce::AudioBuffer<float> playBuffer;

    MixerData mixerData;

    float currentTime = 0;
    int lastSampleIndex = 0;

    std::unordered_set<int> loadedBlocks;
    const float blockDuration = 0.2; // second
    const int sampleRate = 48000;

    void _play();

    void _pause(bool stop);

    void _prepare();

    void _loadTrack(int block);

public:
    JuceMixPlayer();

    ~JuceMixPlayer();

    void play();

    void pause();

    void stop();

    void togglePlayPause();

    void reset(const char* json);

    void prepare();

    void resetItems();

    void setListener(ListenerFunction listener);

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
