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
            onStateUpdateCallback(this, returnCopyCharDelete(JuceMixPlayerState_toString(state)));
        }
    }
}

void JuceMixPlayer::_onErrorNotify(std::string error) {
    if (onErrorCallback != nullptr)
        onErrorCallback(this, returnCopyCharDelete(error));
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
        if (playBuffer.getNumSamples() > 0) {
            _loadAudioBlock(0);
            _onStateUpdateNotify(READY);
        } else {
            _onStateUpdateNotify(ERROR);
        }
    });
}

void JuceMixPlayer::_prepareInternal() {
    for (MixerTrack& track: mixerData.tracks) {
        if (track.enabled) {
            juce::File file(track.path);
            track.reader.reset(formatManager.createReaderFor(file));
            if (!track.reader) {
                _onErrorNotify("unable to read " + track.path);
            }
        }
    }

    float outputDuration = MixerModel::getTotalDuration(mixerData);

    bool keepExistingContent = false;
    bool clearExtraSpace = true;
    bool avoidReallocating = false;
    playBuffer.setSize(2, outputDuration * sampleRate, keepExistingContent, clearExtraSpace, avoidReallocating);
}

std::optional<std::tuple<float, float, float>> JuceMixPlayer::_calculateBlockToRead(float block, MixerTrack& track) {
    if (track.offset > block * blockDuration + blockDuration) {
        PRINT("block <--");
        return std::nullopt;
    }

    float track_duration = track.duration == 0 ? track.reader->lengthInSamples / sampleRate : track.duration;

    if (track.offset + track_duration < block * blockDuration) {
        PRINT("--> block");
        return std::nullopt;
    }

    float diff = track.offset - block * blockDuration;

    float dstStart = std::min(blockDuration, std::max(diff, 0.0f)) * sampleRate;
    float numSamples = blockDuration * sampleRate;
    float readStart = (std::abs(std::min(diff, 0.0f)) + track.fromTime) * sampleRate;

    if (readStart + numSamples > track.reader->lengthInSamples) {
        numSamples = track.reader->lengthInSamples - readStart;
    }
    if (dstStart + numSamples > blockDuration * sampleRate) {
        numSamples = blockDuration * sampleRate - dstStart;
    }
    float lefover = (track.offset + track_duration - block * blockDuration) * sampleRate;
    if (numSamples > lefover) {
        numSamples = lefover;
    }

    return std::tuple(dstStart, numSamples, readStart);
}

void JuceMixPlayer::_loadAudioBlock(int block) {
    if (setContains(loadedBlocks, block)) {
        // block is already loaded
        return;
    }
    if (setContains(loadingBlocks, block)) {
        // already block is getting loaded
        return;
    }
    if (playBuffer.getNumSamples() == 0) {
        _onErrorNotify("output duration is 0");
        return;
    }

    loadingBlocks.insert(block);

    // load the block
    juce::AudioBuffer<float> tempBuffer;
    tempBuffer.setSize(2, blockDuration * sampleRate);

    for (MixerTrack& track: mixerData.tracks) {
        if (!track.enabled) {
            continue;
        }
        if (!track.reader) {
            _onErrorNotify("reader not found for " + track.path);
            continue;
        }

        auto res = _calculateBlockToRead(block, track);
        if (!res.has_value()) {
            continue;
        }

        // clear block buffer
        tempBuffer.clear();

        float dstStart = std::get<0>(res.value());
        float numSamples = std::get<1>(res.value());
        float readStart = std::get<2>(res.value());

        // read data into block buffer
        const bool success = track.reader->read(&tempBuffer, dstStart, numSamples, readStart, true, true);
        if (!success) {
            std::string err = "Read operation was not success for: " + track.path;
            _onErrorNotify(err);
        }

        // mix audio from block buffer
        const float destStartSample = block * blockDuration * sampleRate;
        int sampleCount = blockDuration * sampleRate;
        if (destStartSample + sampleCount > playBuffer.getNumSamples()) {
            sampleCount = playBuffer.getNumSamples() - destStartSample;
        }
        for (int i=0; i<2; i++) {
            playBuffer.addFrom(i, destStartSample, tempBuffer, i, 0, sampleCount, track.volume);
        }
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
