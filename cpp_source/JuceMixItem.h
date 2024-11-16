#pragma once

#include "TaskQueue.h"
#include "Logger.h"
#include <JuceHeader.h>
#include <iostream>

typedef void (*ListenerFunction)(const char*, const char*, float);

enum JuceMixPlayerState
{
    IDLE, PLAYING, PAUSED, STOPPED, READY, LOADING, ERROR
};

const char* JuceMixPlayerStateStringValue(JuceMixPlayerState state);

class JuceMixItem
{
private:
    juce::File file;
    juce::AudioFormatReader* reader;
    juce::AudioBuffer<float> readerBuffer;
    juce::AudioFormatManager formatManager;
    bool isPreparing = false;

public:
    JuceMixItem();

    ~JuceMixItem();

    /// begin time, end time
    void setPath(juce::String path, float begin, float end);

    void prepare();

    bool isLoaded();

    float getNumSamples();

    float getSampleRate();

    juce::AudioBuffer<float>* getReadBuffer();

    std::string toString();
};
