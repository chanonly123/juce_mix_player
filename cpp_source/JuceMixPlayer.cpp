#include "JuceMixPlayer.h"

JuceMixPlayer::JuceMixPlayer() {
    PRINT("JuceMixPlayer()")

    player = new juce::AudioSourcePlayer();
    juce::LinearInterpolator interpolator;

    player->setSource(this);

    juce::MessageManager::getInstance()->callAsync([=] {
        deviceManager = new juce::AudioDeviceManager();
        deviceManager->addAudioCallback(player);
        deviceManager->initialiseWithDefaultDevices(0, 2);
    });

//    mainTaskQueue.postTask([=]() {
//        PRINT("Task 1 is running on the main thread");
//        juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();
//        deviceManager = new juce::AudioDeviceManager();
//        deviceManager->addAudioCallback(player);
//        deviceManager->initialiseWithDefaultDevices(0, 2);
//    });
}

JuceMixPlayer::~JuceMixPlayer() {
    PRINT("~JuceMixPlayer");
    juce::MessageManager::getInstance()->callAsync([=] {
        deviceManager->closeAudioDevice();
        delete player;
        delete deviceManager;
    });
}

void JuceMixPlayer::_play() {
    if (!isPlaying) {
        isPlaying = true;
//        startTimer(100);
    }
}

void JuceMixPlayer::_pause(bool stop) {
    if (isPlaying) {
        isPlaying = false;
//        stopTimer();
    }
    if (stop) {
        lastSampleIndex = 0;
    }
}

void JuceMixPlayer::play() {
    getTaskQueueShared()->async([=] {
        _play();
    });
}

void JuceMixPlayer::pause() {
    getTaskQueueShared()->async([=] {
        _pause(false);
    });
}

void JuceMixPlayer::stop() {
    getTaskQueueShared()->async([=] {
        _pause(true);
    });
}

void JuceMixPlayer::togglePlayPause() {
    getTaskQueueShared()->async([=] {
        if (isPlaying) {
            pause();
        } else {
            play();
        }
    });
}

void JuceMixPlayer::addItem(JuceMixItem* item) {
    getTaskQueueShared()->async([=] {
        tracks.push_back(item);
    });
}

void JuceMixPlayer::resetItems() {
    getTaskQueueShared()->async([=] {
        stop();
        tracks.clear();
    });
}

void JuceMixPlayer::setListener(ListenerFunction listener) {
    this->listener = listener;
}

void JuceMixPlayer::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    this->samplesPerBlockExpected = samplesPerBlockExpected;
    this->deviceSampleRate = sampleRate;
}

void JuceMixPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) {
    if (!isPlaying) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    if (!tracks.empty()) {
        JuceMixItem* item = tracks.at(0);
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

void JuceMixPlayer::releaseResources() {

}

void JuceMixPlayer::timerCallback() {
    PRINT("timer call " << lastSampleIndex);
    if (listener != nullptr) {
        listener("obj", "state", 0.5);
    }
}
