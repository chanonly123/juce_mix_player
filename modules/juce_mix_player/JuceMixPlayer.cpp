#include "JuceMixPlayer.h"

std::string JuceMixPlayerRecState_toString(JuceMixPlayerRecState state) {
    nlohmann::json j = state;
    return j;
}

std::string JuceMixPlayerState_toString(JuceMixPlayerState state) {
    nlohmann::json j = state;
    return j;
}

JuceMixPlayer::JuceMixPlayer() {
    PRINT("JuceMixPlayer()");

    taskQueue.name = "taskQueue";
    recWriteTaskQueue.name = "recWriteTaskQueue";

    formatManager.registerBasicFormats();

    juce::WindowedSincInterpolator interpolator;

    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&, this]{
        if (deviceManager == nullptr) {
            deviceManager = new juce::AudioDeviceManager();
        }
        deviceManager->addAudioCallback(this);
        deviceManager->addChangeListener(this);
        deviceManager->initialise(0, 2, nullptr, true, {}, nullptr);

        juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
        setup.sampleRate = settings.sampleRate;
        bool treatAsChosenDevice = true;
        deviceManager->setAudioDeviceSetup(setup, treatAsChosenDevice);

        inputLevelMeter = deviceManager->getInputLevelGetter();

        PRINT("JuceMixPlayer initialized");
    });
}

void JuceMixPlayer::dispose() {
    PRINT("JuceMixPlayer::dispose");
    taskQueue.stopQueue();
    _stopProgressTimer();
    deviceManager->removeAudioCallback(this);
    deviceManager->removeChangeListener(this);
    stop();
    stopRecorder();
    delete this;
}

JuceMixPlayer::~JuceMixPlayer() {
    PRINT("~JuceMixPlayer");
}

// MARK: Timer

void JuceMixPlayer::_startProgressTimer() {
    if (!isTimerRunning()) {
        startTimer(settings.progressUpdateInterval * 1000);
    }
}

void JuceMixPlayer::_stopProgressTimer() {
    if (isTimerRunning() && !_isPlaying && !_isRecording) {
        stopTimer();
    }
}

void JuceMixPlayer::_playInternal() {
    if (!_isPlaying) {
        _isPlaying = true;
        _onStateUpdateNotify(JuceMixPlayerState::PLAYING);
        _startProgressTimer();
    }
}

void JuceMixPlayer::_pauseInternal(bool stop) {
    if (_isPlaying) {
        _isPlaying = false;
        _stopProgressTimer();
    }
    if (stop) {
        playHeadIndex = 0;
        _onStateUpdateNotify(JuceMixPlayerState::STOPPED);
    } else {
        _onStateUpdateNotify(JuceMixPlayerState::PAUSED);
    }
}

void JuceMixPlayer::play() {
    taskQueue.async([&]{
        _playInternal();
    });
}

void JuceMixPlayer::pause() {
    taskQueue.async([&]{
        _pauseInternal(false);
    });
}

void JuceMixPlayer::stop() {
    taskQueue.async([&]{
        _pauseInternal(true);
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

void JuceMixPlayer::setSettings(const char* json) {
    std::string json_(json);
    taskQueue.async([&, json_]{
        try {
            MixerSettings _settings = MixerModel::parseSettings(json_.c_str());
            settings = _settings;

            juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
                juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
                setup.sampleRate = settings.sampleRate;
                bool treatAsChosenDevice = true;
                deviceManager->setAudioDeviceSetup(setup, treatAsChosenDevice);
            });
        } catch (const std::exception& e) {
            _onErrorNotify(std::string(e.what()));
        }
    });
}

void JuceMixPlayer::prepare() {
    taskQueue.async([&]{
        bool wasPlaying = _isPlaying;
        _pauseInternal(false);
        loadingBlocks.clear();
        loadedBlocks.clear();
        playHeadIndex = 0;
        playBuffer.clear();
        _createFileReadersAndTotalDuration();
        if (playBuffer.getNumSamples() > 0) {
            _loadRepeatedTracks();
            _loadAudioBlock(0);
            _onStateUpdateNotify(JuceMixPlayerState::READY);
            if (wasPlaying) {
                _playInternal();
            }
        } else {
            _onStateUpdateNotify(JuceMixPlayerState::ERROR);
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

// MARK: Recorder

void JuceMixPlayer::prepareRecorder(const char *file) {
    std::string path(file);
    taskQueue.async([&, path]{
        if (_isRecording) {
            if (onRecErrorCallback) onRecErrorCallback(this, "Failed to prepare recorder, stop recorder first");
            return;
        }
        recordPath = path;
        createWriterForRecorder();
    });
}

void JuceMixPlayer::startRecorder(int startPlaying) {
    if (_isRecording) return;
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        if (!_isRecorderPrepared) {
            if (onRecErrorCallback) onRecErrorCallback(this, "Failed to start recording, prepare not called");
            _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
            return;
        }
        deviceManagerSavedState = deviceManager->createStateXml();
        deviceManager->closeAudioDevice();
        deviceManager->initialise(1, 2, deviceManagerSavedState.get(), true, {}, nullptr);
        _isRecording = true;
        _onRecStateUpdateNotify(JuceMixPlayerRecState::RECORDING);
        _startProgressTimer();
        if (startPlaying == 1) {
            play();
        }
    });
}

void JuceMixPlayer::stopRecorder() {
    if (!_isRecording) return;
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        if (_isRecording) {
            stop();
            deviceManagerSavedState = deviceManager->createStateXml();
            deviceManager->closeAudioDevice();
            deviceManager->initialise(0, 2, deviceManagerSavedState.get(), true, {}, nullptr);
            _isRecording = false;
            _stopProgressTimer();
            _finishRecording();
        }
    });
}

void JuceMixPlayer::createWriterForRecorder() {
    if (deviceSampleRate == 0) {
        if (onRecErrorCallback) onRecErrorCallback(this, "deviceSampleRate is 0");
        _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
        return;
    }

    recordBuffer1.setSize(1, recordBufferDuration * 1.5 * deviceSampleRate);
    recordBuffer2.setSize(1, recordBufferDuration * 1.5 * deviceSampleRate);

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
    _onRecStateUpdateNotify(JuceMixPlayerRecState::READY);
    _isRecorderPrepared = true;
}

void JuceMixPlayer::_resetRecorder() {
    _isRecorderPrepared = false;
    recordTimerIndex = 0;
    recordHeadIndex = 0;
    _onRecStateUpdateNotify(JuceMixPlayerRecState::IDLE);
}

void JuceMixPlayer::_finishRecording() {
    recWriteTaskQueue.async([&]{
        juce::AudioBuffer<float>& buff = recordBufferSelect == 0 ? recordBuffer1 : recordBuffer2;
        flushRecordBufferToFile(buff, recordHeadIndex);
        recWriter.reset();
        _onRecStateUpdateNotify(JuceMixPlayerRecState::STOPPED);
        _resetRecorder();
    });
}

void JuceMixPlayer::flushRecordBufferToFile(juce::AudioBuffer<float>& buffer, float sampleCount) {
    if (!recWriter) {
        if (onRecErrorCallback)
            onRecErrorCallback(this, "Failed to write file, writer not created");
        _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
        return;
    }
    if (sampleCount == 0) {
        return;
    }
    bool success = false;
    success = recWriter->writeFromAudioSampleBuffer(buffer, 0, sampleCount);
    success = recWriter->flush();
    if (!success) {
        stopRecorder();
        _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
        if (onRecErrorCallback)
            onRecErrorCallback(this, "Failed to write file");
    }
}

void JuceMixPlayer::_onRecStateUpdateNotify(JuceMixPlayerRecState state) {
    if (onRecStateUpdateCallback != nullptr) {
        if (currentRecState != state) {
            currentRecState = state;
            onRecStateUpdateCallback(this, returnCopyCharDelete(JuceMixPlayerRecState_toString(state)));
        }
    }
}

// MARK: Device management
void JuceMixPlayer::notifyDeviceUpdates() {
    MixerDeviceList list;
    juce::AudioIODeviceType* audioDeviceType = deviceManager->getCurrentDeviceTypeObject();
    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
    if (audioDeviceType) {
        juce::StringArray inputDevices = audioDeviceType->getDeviceNames(true);
        juce::StringArray outputDevices = audioDeviceType->getDeviceNames(false);
        for (juce::String& name : inputDevices) {
            MixerDevice dev;
            dev.name = name.toStdString();
            dev.isInput = true;
            dev.isSelected = setup.inputDeviceName == name;
            list.devices.push_back(dev);
        }
        for (juce::String& name : outputDevices) {
            MixerDevice dev;
            dev.name = name.toStdString();
            dev.isInput = false;
            dev.isSelected = setup.outputDeviceName == name;
            list.devices.push_back(dev);
        }
    }
    taskQueue.async([&, list]{
        if (!(deviceList == list)) {
            deviceList = list;
            nlohmann::json j = list;
            if (onDeviceUpdateCallback)
                onDeviceUpdateCallback(this, returnCopyCharDelete(j.dump(4)));
        }
    });
}

void JuceMixPlayer::setUpdatedDevices(const char* json) {
    std::string json_(json);
    taskQueue.async([&, json_]{
        try {
            MixerDeviceList list = MixerDeviceList::decode(json_);
            if (!(deviceList == list)) {
                deviceList = list;

                MixerDevice inp;
                MixerDevice out;
                for (auto& dev: list.devices) {
                    if (dev.isSelected && dev.isInput && inp.name == "") {
                        inp = dev;
                    }
                    if (dev.isSelected && !dev.isInput && out.name == "") {
                        out = dev;
                    }
                }

                bool hasChanges = inp.name != "" || out.name != "";

                if (!hasChanges) {
                    PRINT("setUpdatedDevices: No selected device found");
                    return;
                }

                PRINT("setUpdatedDevices: selected inp: " << inp.name << ", out: " << out.name);

                juce::MessageManager::getInstanceWithoutCreating()->callAsync([&, inp, out]{
                    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
                    if (inp.name != "") setup.inputDeviceName = juce::String(inp.name);
                    if (out.name != "") setup.outputDeviceName = juce::String(out.name);
                    bool treatAsChosenDevice = true;
                    juce::String err = deviceManager->setAudioDeviceSetup(setup, treatAsChosenDevice);
                    if (err.isNotEmpty()) {
                        _onErrorNotify(err.toStdString());
                    }
                    notifyDeviceUpdates();
                });
            } else {
                PRINT("setUpdatedDevices: Same device data! ignoring");
            }
        } catch (const std::exception& e) {
            _onErrorNotify(std::string(e.what()));
            notifyDeviceUpdates();
        }
    });
}

// MARK: AudioIODeviceCallback
void JuceMixPlayer::audioDeviceAboutToStart(juce::AudioIODevice *device) {
    this->deviceSampleRate = device->getCurrentSampleRate();
    this->samplesPerBlockExpected = device->getCurrentBufferSizeSamples();
    PRINT("audioDeviceAboutToStart: deviceSampleRate: " << deviceSampleRate);
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

    if (_isRecording && numInputChannels > 0) {
        juce::AudioBuffer<float>& buff = recordBufferSelect == 0 ? recordBuffer1 : recordBuffer2;
        float* writer = buff.getWritePointer(0, recordHeadIndex);
        memcpy(writer, inputChannelData[0], (size_t) numSamples * sizeof (float));
        recordHeadIndex += numSamples;
        recordTimerIndex += numSamples;

        if (recordHeadIndex > recordBufferDuration * deviceSampleRate) {
            int sampleCount = recordHeadIndex;
            recWriteTaskQueue.async([&, sampleCount]{
                flushRecordBufferToFile(buff, sampleCount);
            });
            recordHeadIndex = 0;
            recordBufferSelect = 1 - recordBufferSelect;
        }
    }

    if (_isPlaying && numOutputChannels > 0) {
        float speedRatio = sampleRate/deviceSampleRate;
        float readCount = (float)numSamples * speedRatio;

        if (playHeadIndex + readCount > playBuffer.getNumSamples()) {
            if (playBuffer.getNumSamples() == 0) {
                _onStateUpdateNotify(JuceMixPlayerState::IDLE);
            } else {
                _onProgressNotify(1);
                _onStateUpdateNotify(JuceMixPlayerState::COMPLETED);
            }
            _pauseInternal(false);
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
    taskQueue.async([&]{
        if (_isPlaying && playBuffer.getNumSamples() > 0) {
            _onProgressNotify((float)playHeadIndex / (float)playBuffer.getNumSamples());
        }
        if (_isRecording) {
            if (onRecProgressCallback) {
                onRecProgressCallback(this, (float)recordTimerIndex / (float)deviceSampleRate);
            }
            if (onRecLevelCallback) {
                onRecLevelCallback(this, (float)inputLevelMeter->getCurrentLevel());
            }
        }
    });
}

void JuceMixPlayer::changeListenerCallback(juce::ChangeBroadcaster* source) {
    PRINT("changeListenerCallback");
    notifyDeviceUpdates();
}
