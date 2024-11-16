#pragma once

#include "Logger.h"
#include "TaskQueue.h"
#include "JuceMixItem.h"
#include <JuceHeader.h>
#include <iostream>

class JuceMixPlayer : public juce::AudioSource,
                      private juce::Timer
{
private:
    std::vector<JuceMixItem*> tracks;
    int samplesPerBlockExpected = 0;
    int deviceSampleRate = 0;
    juce::LinearInterpolator interpolator[5];
    juce::AudioDeviceManager* deviceManager;
    juce::AudioSourcePlayer* player;
    JuceMixPlayerState state = IDLE;
    ListenerFunction listener = nullptr;
    bool isPlaying = false;

    float currentTime = 0;
    int lastSampleIndex = 0;

    void _play();

    void _pause(bool stop);

public:
    JuceMixPlayer();

    ~JuceMixPlayer();

    void play();

    void pause();

    void stop();

    void togglePlayPause();

    void addItem(JuceMixItem* item);

    void resetItems();

    void setListener(ListenerFunction listener);

    /// juce::AudioSource
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override;

    void releaseResources() override;

    /// juce::Timer
    void timerCallback() override;
};
