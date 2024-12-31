#include "JuceMixPlayer.h"

JuceMixPlayerRef::JuceMixPlayerRef() {
    ptr.reset(new JuceMixPlayer());
}

JuceMixPlayer::JuceMixPlayer() {
    PRINT("JuceMixPlayer()");

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
        startTimer(1000);
    }
}

void JuceMixPlayer::_pause(bool stop) {
    if (isPlaying) {
        isPlaying = false;
        stopTimer();
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

void JuceMixPlayer::_onProgressNotify(float progress) {
    if (onProgressCallback != nullptr)
        onProgressCallback(progress);
}

void JuceMixPlayer::_onStateUpdateNotify(std::string state) {
    if (onStateUpdateCallback != nullptr)
        onStateUpdateCallback(returnCopyCharDelete(state.c_str()));
}

void JuceMixPlayer::togglePlayPause() {
    if (isPlaying) {
        pause();
    } else {
        play();
    }
}

void JuceMixPlayer::set(const char* json) {
    taskQueue.async([&, json]{
        try {
            MixerData data = MixerModel::parse(json);
            if (!(mixerData == data)) {
                mixerData = data;
                prepare();
            }
        } catch (const std::exception& e) {
            _onStateUpdateNotify("Error: " + std::string(e.what()));
        }
    });
}

void JuceMixPlayer::prepare() {
    taskQueue.async([&]{
        _prepare();
        _loadAudioBlock(0);
    });
}

void JuceMixPlayer::_prepare() {
    float outputDuration = mixerData.outputDuration;
    for (MixerTrack& track: mixerData.tracks) {
        if (track.enabled) {
            juce::File file(track.path);
            track.reader.reset(formatManager.createReaderFor(file));

            if (track.reader) {
                // if outputDuration is zero, make it dynamic
                if (mixerData.outputDuration == 0) {
                    float dur = (float)track.reader->lengthInSamples / (float)track.reader->sampleRate;
                    if (dur > outputDuration) {
                        outputDuration = dur;
                    }
                }
            } else {
                _onStateUpdateNotify("ERROR: " + track.path + " not able to read");
            }
        }
    }

    bool keepExistingContent = false;
    bool clearExtraSpace = true;
    bool avoidReallocating = false;
    playBuffer.setSize(2, outputDuration * sampleRate, keepExistingContent, clearExtraSpace, avoidReallocating);
}

void JuceMixPlayer::_loadAudioBlock(int block) {
    if (setContains(loadingBlocks, block)) {
        // already block is getting loaded
        return;
    }
    loadingBlocks.insert(block);
    if (playBuffer.getNumSamples() == 0) {
        _onStateUpdateNotify("ERROR: output duration is 0");
        return;
    }
    if (setContains(loadedBlocks, block)) {
        // block is already loaded
        return;
    }
    // load the block
    float startTime = block * blockDuration;
    for (MixerTrack& track: mixerData.tracks) {
        if (track.enabled && track.reader) {
            track.reader->read(&playBuffer, startTime * sampleRate, blockDuration * sampleRate, startTime * sampleRate, true, true);
        }
    }
    loadedBlocks.insert(block);
    loadingBlocks.erase(block);
}

void JuceMixPlayer::onProgress(JuceMixPlayerOnProgress callback) {
    onProgressCallback = callback;
}

void JuceMixPlayer::onStateUpdate(JuceMixPlayerOnStateUpdate callback) {
    onStateUpdateCallback = callback;
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

    float speedRatio = sampleRate/deviceSampleRate;
    float readCount = (float)bufferToFill.numSamples * speedRatio;

    for (int i=0; i<playBuffer.getNumChannels(); i++) {
        float* buffer = bufferToFill.buffer->getWritePointer(i, bufferToFill.startSample);
        interpolator[i].process(speedRatio,
                                playBuffer.getReadPointer(i, lastSampleIndex),
                                buffer,
                                bufferToFill.numSamples);
    }

    lastSampleIndex += readCount;
    if (lastSampleIndex > playBuffer.getNumSamples()) {
        _pause(false);
    }

    // load next block
    taskQueue.async([&]{
        _loadAudioBlock((getCurrentTime()/blockDuration)+1);
    });
}

float JuceMixPlayer::getCurrentTime() {
    return lastSampleIndex / sampleRate;
}

void JuceMixPlayer::releaseResources() {

}

void JuceMixPlayer::timerCallback() {
    _onProgressNotify(getCurrentTime());
}
