#include "JuceMixPlayer.h"

JuceMixPlayerRef::JuceMixPlayerRef() {
    ptr.reset(new JuceMixPlayer());
}

JuceMixPlayer::JuceMixPlayer() {
    PRINT("JuceMixPlayer()")

    formatManager.registerBasicFormats();

    player = new juce::AudioSourcePlayer();
    juce::LinearInterpolator interpolator;

    player->setSource(this);

    deviceManager = new juce::AudioDeviceManager();
    deviceManager->addAudioCallback(player);
    deviceManager->initialiseWithDefaultDevices(0, 2);
}

JuceMixPlayer::~JuceMixPlayer() {
    PRINT("~JuceMixPlayer");
    deviceManager->closeAudioDevice();
    delete player;
    delete deviceManager;
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
    _play();
}

void JuceMixPlayer::pause() {
    _pause(false);
}

void JuceMixPlayer::stop() {
    _pause(true);
}

void JuceMixPlayer::togglePlayPause() {
    if (isPlaying) {
        pause();
    } else {
        play();
    }
}

void JuceMixPlayer::reset(const char* json) {
    try {
        MixerData data = MixerModel::parse(json);
        if (!(mixerData == data)) {
            taskQueue.async([&, data]{
                mixerData = data;
                prepare();
            });
        }
    } catch (const std::exception& e) {
        throw std::string(e.what());
    }
}

void JuceMixPlayer::prepare() {
    taskQueue.async([&]{
        _prepare();
        _loadTrack(0);
    });
}

void JuceMixPlayer::_prepare() {
    bool keepExistingContent = false;
    bool clearExtraSpace = true;
    bool avoidReallocating = false;
    playBuffer.setSize(2, mixerData.outputDuration * sampleRate, keepExistingContent, clearExtraSpace, avoidReallocating);
    for (MixerTrack track: mixerData.tracks) {
        track.reader = std::make_shared<juce::AudioFormatReader>(formatManager.createReaderFor(juce::File(track.path)));
    }
}

void JuceMixPlayer::_loadTrack(int block) {
    if (loadedBlocks.find(block) != loadedBlocks.end()) {
        return;
    }
    float startTime = block * blockDuration;
    for (MixerTrack track: mixerData.tracks) {
        track.reader->read(&playBuffer, startTime * sampleRate, blockDuration * sampleRate, startTime * sampleRate, true, true);
    }
    loadedBlocks.insert(block);
}

void JuceMixPlayer::resetItems() {
    TaskQueue::shared.async([&] {
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

    float speedRatio = (float)sampleRate/(float)deviceSampleRate;
    int readCount = bufferToFill.numSamples * speedRatio;

    for (int i=0; i<playBuffer.getNumChannels(); i++) {
        float* buffer = bufferToFill.buffer->getWritePointer(i, bufferToFill.startSample);
        interpolator[i].process(speedRatio,
                                playBuffer.getReadPointer(i, lastSampleIndex),
                                buffer,
                                bufferToFill.numSamples);
    }

    lastSampleIndex += readCount;

    //    if (!tracks.empty()) {
    //        JuceMixItem* item = tracks.at(0);
    //        item->prepare();
    //        if (item->isLoaded()) {
    //            if (lastSampleIndex < item->getNumSamples()) {
    //                float speedRatio = item->getSampleRate()/deviceSampleRate;
    //                int readCount = bufferToFill.numSamples * speedRatio;
    //
    //                for (int i=0; i<item->getReadBuffer()->getNumChannels() && i<bufferToFill.buffer->getNumChannels(); i++) {
    //
    //                    float* buffer = bufferToFill.buffer->getWritePointer(i, bufferToFill.startSample);
    //                    interpolator[i].process(speedRatio,
    //                                            item->getReadBuffer()->getReadPointer(i, lastSampleIndex),
    //                                            buffer,
    //                                            bufferToFill.numSamples);
    //                }
    //
    //                lastSampleIndex += readCount;
    //            } else {
    //                stop();
    //            }
    //        }
    //    }
}

void JuceMixPlayer::releaseResources() {

}

void JuceMixPlayer::timerCallback() {
    PRINT("timer call " << lastSampleIndex);
    if (listener != nullptr) {
        listener("obj", "state", 0.5);
    }
}
