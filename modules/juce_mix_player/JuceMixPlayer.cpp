#include "JuceMixPlayer.h"

std::string JuceMixPlayerState_toString(JuceMixPlayerState state) {
    nlohmann::json j = state;
    return j;
}

JuceMixPlayerState JuceMixPlayerState_make(std::string state) {
    nlohmann::json j = state;
    return j.template get<JuceMixPlayerState>();
}

JuceMixPlayer::JuceMixPlayer(int record, int play) {
    PRINT("JuceMixPlayer() record: " << record << ", play: " << play);

    formatManager.registerBasicFormats();

    juce::WindowedSincInterpolator interpolator;

    taskQueue.async([&, this, record, play]{
        if (record == 1) {
            // allocate record buffer for 10 minutes, with single channel
            recordBuffer.setSize(1, 10*60*sampleRate);
        }
        juce::MessageManager::getInstanceWithoutCreating()->callAsync([&, this, record, play]{
            deviceManager = new juce::AudioDeviceManager();
            deviceManager->addAudioCallback(this);
            deviceManager->initialiseWithDefaultDevices(record == 1 ? 1 : 0, play == 1 ? 2 : 0);


            juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
            setup.sampleRate = 48000;
            deviceManager->setAudioDeviceSetup(setup, true);
        });
    });
}

void JuceMixPlayer::dispose() {
    taskQueue.async([&, this]{
        PRINT("JuceMixPlayer::dispose");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        juce::MessageManager::getInstanceWithoutCreating()->callAsync([&] {
            deviceManager->removeAudioCallback(this);
            deviceManager->closeAudioDevice();
            stopTimer();
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
        playHeadIndex = 0;
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
        playHeadIndex = playBuffer.getNumSamples() * _value;
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
        bool wasPlaying = _isPlaying;
        _pause(false);
        loadingBlocks.clear();
        loadedBlocks.clear();
        playHeadIndex = 0;
        playBuffer.clear();
        _createFileReadersAndTotalDuration();
        if (playBuffer.getNumSamples() > 0) {
            _loadRepeatedTracks();
            _loadAudioBlock(0);
            _onStateUpdateNotify(READY);
            if (wasPlaying) {
                _play();
            }
        } else {
            _onStateUpdateNotify(ERROR);
        }
    });
}

void JuceMixPlayer::_createFileReadersAndTotalDuration() {
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

void JuceMixPlayer::_loadRepeatedTracks() {
    std::unordered_map<std::string, std::shared_ptr<juce::AudioBuffer<float>>> buffers;
    for (MixerTrack& track: mixerData.tracks) {
        if (!track.repeat || !track.reader || !track.enabled) {
            continue;
        }
        int sampleCount = (int)track.reader->lengthInSamples;
        if (sampleCount == 0) {
            continue;
        }
        std::shared_ptr<juce::AudioBuffer<float>> buff;
        if (buffers.find(track.path) == buffers.end()) {
            buff.reset(new juce::AudioBuffer<float>(2, sampleCount));
            track.reader->read(buff.get(), 0, sampleCount, 0, true, true);
            buffers[track.path] = buff;
        } else {
            buff = buffers.at(track.path);
        }

        int destStartSample = track.offset * sampleRate;

        while (true) {
            const int sourceStartSample = track.fromTime * sampleRate;
            int numSamples = track.duration == 0 ? buff->getNumSamples() : track.duration * sampleRate;
            const int totalSampleCount = destStartSample + numSamples;
            if (totalSampleCount > playBuffer.getNumSamples()) {
                break;
            }
            for (int i = 0; i < buff->getNumChannels(); i++) {
                if (sourceStartSample + numSamples > buff->getNumSamples()) {
                    numSamples = buff->getNumSamples() - sourceStartSample;
                }
                playBuffer.addFrom(i, destStartSample, *buff.get(), i, sourceStartSample, numSamples, track.volume);
            }
            destStartSample += track.repeatInterval * sampleRate;
        }
    }
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
        if (!track.enabled || track.repeat) {
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

float JuceMixPlayer::getCurrentTime() {
    if (sampleRate == 0) {
        return 0;
    }
    return (float)playHeadIndex / sampleRate;
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

void JuceMixPlayer::startRecorder(const char* file) {
    std::string path(file);
    taskQueue.async([&, path]{
        recordPath = path;
        _isRecording = true;
    });
}

void JuceMixPlayer::stopRecorder() {
    taskQueue.async([&]{
        _isRecording = false;
        writeBufferToFile(recordBuffer, juce::String(recordPath), deviceSampleRate, 1, recordHeadIndex, "wav");
    });
}

void JuceMixPlayer::createWriterForRecorder() {
    std::string format = "wav";
    int targetSampleRate = deviceSampleRate;
    juce::File outputFile(recordPath);
    outputFile.deleteFile();
    juce::FileOutputStream* outputStream = new juce::FileOutputStream(outputFile);
    if (format == "wav") {
        recAudioFormat.reset(new juce::WavAudioFormat());
    } else if (format == "flac") {
        recAudioFormat.reset(new juce::FlacAudioFormat());
    }
    recWriter.reset(recAudioFormat->createWriterFor(outputStream, targetSampleRate, 1, 16, {}, 0));
}

void JuceMixPlayer::writeRecChunk() {
    
}

bool JuceMixPlayer::writeBufferToFile(juce::AudioBuffer<float>& buffer,
                                      const juce::String& outputFilePath,
                                      double targetSampleRate,
                                      int numChannels,
                                      int sampleCount,
                                      std::string format) {
    // Write the processed buffer to the output file
    juce::File outputFile(outputFilePath);
    outputFile.deleteFile();
    juce::FileOutputStream* outputStream = new juce::FileOutputStream(outputFile);
    std::unique_ptr<juce::AudioFormat> audioFormat;
    if (format == "wav") {
        audioFormat.reset(new juce::WavAudioFormat());
    } else if (format == "flac") {
        audioFormat.reset(new juce::FlacAudioFormat());
    }
    std::unique_ptr<juce::AudioFormatWriter> writer(audioFormat->createWriterFor(outputStream, targetSampleRate, numChannels, 16, {}, 0));
    bool success = false;
    if (writer != nullptr) {
        success = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        success = writer->flush();
    } else {
        PRINT("Failed to create writer for output file.");
    }
    writer.reset();
    PRINT("writeBufferToFile: success: " << (success ? 1 : 0));
    return success;
}

// MARK: AudioIODeviceCallback
void JuceMixPlayer::audioDeviceAboutToStart(juce::AudioIODevice *device) {
    this->deviceSampleRate = device->getCurrentSampleRate();
    this->samplesPerBlockExpected = device->getCurrentBufferSizeSamples();
}

void JuceMixPlayer::audioDeviceIOCallbackWithContext(const float *const *inputChannelData,
                                                     int numInputChannels,
                                                     float *const *outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext &context) {
    if (deviceSampleRate <= 0) {
        return;
    }

    const juce::ScopedLock sl (lock);

    if (_isRecording) {
        float* writer = recordBuffer.getWritePointer(0, recordHeadIndex);
        memcpy(writer, inputChannelData[0], (size_t) numSamples * sizeof (float));
        recordHeadIndex += numSamples;
    }

    if (_isPlaying) {
        float speedRatio = sampleRate/deviceSampleRate;
        float readCount = (float)numSamples * speedRatio;

        if (playHeadIndex + readCount > playBuffer.getNumSamples()) {
            if (playBuffer.getNumSamples() == 0) {
                _onStateUpdateNotify(IDLE);
            } else {
                _onProgressNotify(1);
                _onStateUpdateNotify(COMPLETED);
            }
            _pause(false);
            return;
        }

        for (int ch=0; ch<numOutputChannels; ch++) {
            interpolator[ch].process(speedRatio,
                                     playBuffer.getReadPointer(ch, playHeadIndex),
                                     outputChannelData[ch],
                                     numSamples);
        }

        playHeadIndex += readCount;

        // load next block in advance
        taskQueue.async([&]{
            _loadAudioBlock((getCurrentTime()/blockDuration)+1);
        });
    } else {
        for (int ch=0; ch<numOutputChannels; ch++) {
            juce::zeromem(outputChannelData[ch], (size_t) numSamples * sizeof (float));
        }
    }
}

void JuceMixPlayer::audioDeviceError(const juce::String &errorMessage) {
    PRINT("audioDeviceError: " << errorMessage);
}

void JuceMixPlayer::audioDeviceStopped() {
    PRINT("audioDeviceStopped: ");
}

// MARK: juce::Timer
void JuceMixPlayer::timerCallback() {
    if (_isPlaying && playBuffer.getNumSamples() > 0) {
        _onProgressNotify((float)playHeadIndex / (float)playBuffer.getNumSamples());
    }
}
