#include "JuceMixPlayer.h"

std::string JuceMixPlayerState_toString(JuceMixPlayerState state) {
    nlohmann::json j = state;
    return j;
}

JuceMixPlayerState JuceMixPlayerState_make(std::string state) {
    nlohmann::json j = state;
    return j.template get<JuceMixPlayerState>();
}

JuceMixPlayer::JuceMixPlayer() {
    PRINT("JuceMixPlayer()");

    formatManager.registerBasicFormats();

    player = new juce::AudioSourcePlayer();
    juce::WindowedSincInterpolator interpolator;

    player->setSource(this);

    taskQueue.async([&]{
        juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
            deviceManager = new juce::AudioDeviceManager();
            deviceManager->addAudioCallback(player);
            deviceManager->initialiseWithDefaultDevices(0, 2);
        });
    });
}

void JuceMixPlayer::dispose() {
    taskQueue.async([&]{
        std::this_thread::sleep_for(std::chrono::seconds(2));
        juce::MessageManager::getInstanceWithoutCreating()->callAsync([&] {
            deviceManager->removeAudioCallback(player);
            deviceManager->closeAudioDevice();
            stopTimer();
            delete player;
            delete deviceManager;
            delete this;
        });
    });
}

JuceMixPlayer::~JuceMixPlayer() {
    PRINT("~JuceMixPlayer");
}

void JuceMixPlayer::_play() {
    if (!_isPlaying) {
        _isPlaying = true;
        _onStateUpdateNotify(PLAYING);
        startTimer(progressUpdateInterval * 1000);
    }
}

void JuceMixPlayer::_pause(bool stop) {
    if (_isPlaying) {
        _isPlaying = false;
        stopTimer();
    }
    if (stop) {
        lastSampleIndex = 0;
        _onStateUpdateNotify(STOPPED);
    } else {
        _onStateUpdateNotify(PAUSED);
    }
}

void JuceMixPlayer::play() {
    taskQueue.async([&]{
        _play();
    });
}

void JuceMixPlayer::pause() {
    taskQueue.async([&]{
        _pause(false);
    });
}

void JuceMixPlayer::stop() {
    taskQueue.async([&]{
        _pause(true);
    });
}

void JuceMixPlayer::seek(float value) {
    float _value = std::min(1.0f, std::max(value, 0.0f));
    taskQueue.async([&, _value]{
        bool wasPlaying = _isPlaying;
        _isPlaying = false;
        _loadAudioBlock(getDuration() * _value / blockDuration);
        lastSampleIndex = playBuffer.getNumSamples() * _value;
        if (wasPlaying) {
            _isPlaying = true;
        }
    });
}

void JuceMixPlayer::_onProgressNotify(float progress) {
    if (onProgressCallback != nullptr)
        onProgressCallback(this, std::min(progress, 1.0F));
}

void JuceMixPlayer::_onStateUpdateNotify(JuceMixPlayerState state) {
    if (onStateUpdateCallback != nullptr) {
        if (currentState != state) {
            currentState = state;
            onStateUpdateCallback(this, returnCopyCharDelete(JuceMixPlayerState_toString(state).c_str()));
        }
    }
}

void JuceMixPlayer::_onErrorNotify(std::string error) {
    if (onErrorCallback != nullptr)
        onErrorCallback(this, returnCopyCharDelete(error.c_str()));
}

void JuceMixPlayer::togglePlayPause() {
    if (_isPlaying) {
        pause();
    } else {
        play();
    }
}

void JuceMixPlayer::setJson(const char* json) {
    std::string json_(json);
    taskQueue.async([&, json_]{
        try {
            MixerData data = MixerModel::parse(json_.c_str());
            if (!(mixerData == data)) {
                mixerData = data;
                prepare();
            } else {
                PRINT("Same mix data! ignoring");
            }
        } catch (const std::exception& e) {
            mixerData = MixerData();
            prepare();
            PRINT(e.what());
            _onErrorNotify(std::string(e.what()));
        }
    });
}

void JuceMixPlayer::prepare() {
    taskQueue.async([&]{
        loadingBlocks.clear();
        loadedBlocks.clear();
        lastSampleIndex = 0;
        _prepareInternal();
        _loadAudioBlock(0);
        _onStateUpdateNotify(READY);
    });
}

void JuceMixPlayer::_prepareInternal() {
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
                _onErrorNotify("unable to read " + track.path);
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
        _onErrorNotify("output duration is 0");
        return;
    }
    if (setContains(loadedBlocks, block)) {
        // block is already loaded
        return;
    }
    // load the block
    const float startTime = block * blockDuration;

    const int startSampleInDestBuffer = startTime * sampleRate;
    const int numSamples = blockDuration * sampleRate;
    const int readerStartSample = startTime * sampleRate;

    for (MixerTrack& track: mixerData.tracks) {
        if (!track.enabled) {
            continue;
        }
        if (!track.reader) {
            _onErrorNotify("reader not found for " + track.path);
            continue;
        }
        if (readerStartSample > track.reader->lengthInSamples) {
            continue;
        }

        int numSamplesFinal = numSamples;
        if (readerStartSample + numSamples > track.reader->lengthInSamples) {
            numSamplesFinal = (int)track.reader->lengthInSamples - readerStartSample;
        }

        track.reader->read(&playBuffer,
                           startSampleInDestBuffer,
                           numSamplesFinal,
                           readerStartSample,
                           true,
                           true);
    }
    loadedBlocks.insert(block);
    loadingBlocks.erase(block);
}

void JuceMixPlayer::onProgress(JuceMixPlayerCallbackFloat callback) {
    onProgressCallback = callback;
}

void JuceMixPlayer::onStateUpdate(JuceMixPlayerCallbackString callback) {
    onStateUpdateCallback = callback;
}

void JuceMixPlayer::onError(JuceMixPlayerCallbackString callback) {
    onErrorCallback = callback;
}

// override
void JuceMixPlayer::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    this->samplesPerBlockExpected = samplesPerBlockExpected;
    this->deviceSampleRate = sampleRate;
}

// override
void JuceMixPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) {
    if (!_isPlaying) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    float speedRatio = sampleRate/deviceSampleRate;
    float readCount = (float)bufferToFill.numSamples * speedRatio;

    if (lastSampleIndex + readCount > playBuffer.getNumSamples()) {
        if (playBuffer.getNumSamples() == 0) {
            _onStateUpdateNotify(IDLE);
        } else {
            _onProgressNotify(1);
            _onStateUpdateNotify(COMPLETED);
        }
        _pause(false);
        return;
    }

    for (int i=0; i<playBuffer.getNumChannels(); i++) {
        float* buffer = bufferToFill.buffer->getWritePointer(i, bufferToFill.startSample);
        interpolator[i].process(speedRatio,
                                playBuffer.getReadPointer(i, lastSampleIndex),
                                buffer,
                                bufferToFill.numSamples);
    }

    lastSampleIndex += readCount;

    // load next block in advance
    taskQueue.async([&]{
        _loadAudioBlock((getCurrentTime()/blockDuration)+1);
    });
}

// override
void JuceMixPlayer::releaseResources() {

}

// override
void JuceMixPlayer::timerCallback() {
    if (_isPlaying && playBuffer.getNumSamples() > 0) {
        _onProgressNotify(lastSampleIndex / (float)playBuffer.getNumSamples());
    }
}

float JuceMixPlayer::getCurrentTime() {
    return lastSampleIndex / sampleRate;
}

float JuceMixPlayer::getDuration() {
    return playBuffer.getNumSamples() / sampleRate;
}

std::string JuceMixPlayer::getCurrentState() {
    return JuceMixPlayerState_toString(currentState);
}

int JuceMixPlayer::isPlaying() {
    return _isPlaying ? 1 : 0;
}

void JuceMixPlayer::setProgressUpdateInterval(float time) {
    progressUpdateInterval = time;
}
