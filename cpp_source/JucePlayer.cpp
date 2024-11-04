#pragma once

#include "TaskQueue.cpp"
#include "Logger.cpp"
#include <JuceHeader.h>
#include <iostream>

TaskQueue taskQueue;

void _initMessageThread() {
    juce::MessageManager::getInstance();
}

enum JuceMixPlayerState {
    IDLE, PLAYING, PAUSED, STOPPED, READY, LOADING, ERROR
};

class JuceMixItem {
private:
    juce::File file;
    juce::AudioFormatReader* reader;
    float lastSampleIndex = 0;
    juce::AudioBuffer<float> readerBuffer;
    juce::AudioFormatManager formatManager;

    bool isPreparing = false;

public:
    JuceMixItem() {
        formatManager.registerBasicFormats();
    }

    ~JuceMixItem() {
        PRINT("~JuceMixItem");
        delete reader;
    }

    void setPath(juce::String path) {
        file = juce::File(path);
        readerBuffer = juce::AudioBuffer<float>();
        isPreparing = false;
    }

    void prepare() {
        if (isLoaded()) {
            return;
        }
        if (isPreparing) {
            return;
        }
        isPreparing = true;
        taskQueue.async([=]{
            if (file.existsAsFile()) {
                reader = formatManager.createReaderFor(file);
                readerBuffer = juce::AudioBuffer<float>((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&readerBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
                PRINT("JuceMixItem load: " << file.getFileName() << ", samples: " << reader->lengthInSamples << ", rate: " << reader->sampleRate);
            } else {
                PRINT("JuceMixItem load: " << file.getFileName() << "file not exists");
            }
            isPreparing = false;
        });
    }

    bool isLoaded() {
        return readerBuffer.getNumSamples() > 0;
    }

    float getNumSamples() {
        return readerBuffer.getNumSamples();
    }

    float getSampleRate() {
        return reader != nullptr ? reader->sampleRate : 0;
    }

    juce::AudioBuffer<float>* getReadBuffer() {
        return &readerBuffer;
    }
};

class JuceMixPlayer : public juce::AudioSource {
private:
    std::vector<JuceMixItem*> items;
    int samplesPerBlockExpected = 0;
    int deviceSampleRate = 0;
    juce::LinearInterpolator interpolator[5];
    juce::AudioDeviceManager* deviceManager;
    juce::AudioSourcePlayer* player;
    JuceMixPlayerState state = IDLE;

    bool isPlaying = false;

    float currentTime = 0;
    int lastSampleIndex = 0;

    void _play() {
        if (!isPlaying) {
            isPlaying = true;
        }
    }

    void _pause(bool stop) {
        if (isPlaying) {
            isPlaying = false;
        }
        if (stop) {
            lastSampleIndex = 0;
        }
    }

public:
    JuceMixPlayer() {
        juce::MessageManager::getInstance();

        player = new juce::AudioSourcePlayer();
        deviceManager = new juce::AudioDeviceManager();
        juce::LinearInterpolator interpolator;

        player->setSource(this);
        deviceManager->addAudioCallback(player);

        taskQueue.async([=] {
            deviceManager->initialiseWithDefaultDevices(0, 2);
        });
    }

    ~JuceMixPlayer() {
        PRINT("~JuceMixPlayer");
        delete player;
        delete deviceManager;
    }

    void play() {
        taskQueue.async([=] {
            _play();
        });
    }

    void pause() {
        taskQueue.async([=] {
            _pause(false);
        });
    }

    void stop() {
        taskQueue.async([=] {
            _pause(true);
        });
    }

    void togglePlayPause() {
        taskQueue.async([=] {
            if (isPlaying) {
                pause();
            } else {
                play();
            }
        });
    }

    void addItem(JuceMixItem* item) {
        taskQueue.async([=] {
            items.push_back(item);
        });
    }

    void resetItems() {
        taskQueue.async([=] {
            stop();
            items.clear();
        });
    }

    // AudioSource delegates
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        this->samplesPerBlockExpected = samplesPerBlockExpected;
        this->deviceSampleRate = sampleRate;
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override {
        if (!isPlaying) {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        if (!items.empty()) {
            JuceMixItem* item = items.at(0);
            item->prepare();
            if (item->isLoaded()) {
                if (lastSampleIndex < item->getNumSamples()) {
                    float speedRatio = item->getSampleRate()/deviceSampleRate;
                    int readCount = bufferToFill.numSamples * speedRatio;

                    for (int i=0; i<item->getReadBuffer()->getNumChannels() && i<bufferToFill.buffer->getNumChannels(); i++) {

                        float* buffer = bufferToFill.buffer->getWritePointer(i, bufferToFill.startSample);
                        interpolator[i].process(speedRatio,
                                             item->getReadBuffer()->getReadPointer(i, lastSampleIndex),
                                             buffer,
                                             bufferToFill.numSamples);
                    }

                    lastSampleIndex += readCount;
                } else {
                    stop();
                }
            }
        }
    }

    void releaseResources() override {

    }
};
