#include "JuceMixPlayer.h"

#if JUCE_IOS

#import <AVFoundation/AVFoundation.h>

// MARK: set audio session for iOS
bool setAudioSessionPlay() {
    NSUInteger options = AVAudioSessionCategoryOptionMixWithOthers;
    NSError* error = nil;
    [[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryPlayback
                                     withOptions: options
                                           error: &error];
    return error == nil;
}

bool setAudioSessionRecord() {
    NSUInteger options = AVAudioSessionCategoryOptionDefaultToSpeaker
    | AVAudioSessionCategoryOptionAllowBluetoothA2DP
    | AVAudioSessionCategoryOptionAllowBluetoothHFP;
    NSError* error = nil;
    [[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryPlayAndRecord
                                     withOptions: options
                                           error: &error];
    return error == nil;
}
#elif JUCE_MAC
bool setAudioSessionPlay() { return true; }
bool setAudioSessionRecord() { return true; }
#else
bool setAudioSessionPlay() { return true; }
bool setAudioSessionRecord() { return true; }
#endif

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
    heavyTaskQueue.name = "heavyTaskQueue";
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

        //        setDefaultSampleRate();

        inputLevelMeter = deviceManager->getInputLevelGetter();

        PRINT("JuceMixPlayer initialized");
    });
}

void JuceMixPlayer::dispose() {
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        PRINT("JuceMixPlayer::dispose");
        _stopProgressTimer();
        deviceManager->removeAudioCallback(this);
        deviceManager->removeChangeListener(this);
        stop();
        stopRecorder();
        std::thread thread([&]{
            taskQueue.stopQueue();
            heavyTaskQueue.stopQueue();
            juce::Thread::sleep(5000);
            delete this;
        });
        thread.detach();
    });
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
        _isPlayingInternal = true;
        _onStateUpdateNotify(JuceMixPlayerState::PLAYING);
        _startProgressTimer();
    }
}

void JuceMixPlayer::_pauseInternal(bool stop) {
    if (_isPlaying) {
        _isPlaying = false;
        _isPlayingInternal = false;
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
    if (_isExporting) {
        _onErrorNotify("Playing is not supported while exporting");
        return;
    }
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
    if (_isExporting) {
        _onErrorNotify("Seeking is not supported while exporting");
        return;
    }
    float _value = std::min(1.0f, std::max(value, 0.0f));
    taskQueue.async([&, _value] {
        _isSeeking = true;
        playHeadIndex = playBuffer.getNumSamples() * _value;
        _loadAudioBlockSafe(getDuration() * _value / blockDuration, false, [&] {
            _isSeeking = false;
        });
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
    if (_isExporting) {
        _onErrorNotify("setJson is not supported while exporting");
        return;
    }
    std::string json_(json);
    taskQueue.async([&, json_]{
        try {
            MixerData data = MixerModel::parse(json_.c_str());
            if (!(mixerData == data)) {
                PRINT("Replacing mix data!")
                mixerData = data;
                _prepare();
            } else {
                PRINT("Same mix data! updating volume/offset/fromTime" << json_);
                _copyReaders(mixerData, data);
                mixerData = data;
                _resetPlayBufferBlocks();
            }
        } catch (const std::exception& e) {
            mixerData = MixerData();
            _prepare();
            _onErrorNotify(std::string(e.what()));
        }
    });
}

void JuceMixPlayer::resetPlayBuffer() {
    taskQueue.async([&]{
        _resetPlayBufferBlocks();
    });
}

void JuceMixPlayer::setSettings(const char* json) {
    if (_isExporting) {
        _onErrorNotify("setSettings is not supported while exporting");
        return;
    }
    std::string json_(json);
    PRINT("setSettings: " << json);

    taskQueue.async([&, json_]{
        try {
            MixerSettings _settings = MixerModel::parseSettings(json_.c_str());
            settings = _settings;

            juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
                juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
                setup.sampleRate = settings.sampleRate;
                bool treatAsChosenDevice = false;
                juce::String error = deviceManager->setAudioDeviceSetup(setup, treatAsChosenDevice);
                if (error.isNotEmpty()) {
                    PRINT("setSettings: " << error);
                    _onErrorNotify(error.toStdString());
                }
            });
        } catch (const std::exception& e) {
            _onErrorNotify(std::string(e.what()));
        }
    });
}

void JuceMixPlayer::_prepare() {
    taskQueue.async([&]{
        _isPlayingInternal = false;
        _pauseInternal(false);
        playHeadIndex = 0;
        _createFileReadersAndTotalDuration();
        if (playBuffer.getNumSamples() > 0) {
            _loadAudioBlockSafe(0, true, [&]{
                _onStateUpdateNotify(JuceMixPlayerState::READY);
                _isPlayingInternal = true;
                if (_isPlaying) {
                    _playInternal();
                }
            });
        } else {
            _onStateUpdateNotify(JuceMixPlayerState::ERROR);
        }
    });
}

void JuceMixPlayer::_resetPlayBufferBlocks() {
    _isPlayingInternal = false;
    _loadAudioBlockSafe((getCurrentTime()/blockDuration), true, [&] {
        _isPlayingInternal = true;
    });
}

void JuceMixPlayer::_copyReaders(const MixerData& from, MixerData& to) {
    for (const MixerTrack& fromTrack: from.tracks) {
        for (MixerTrack& toTrack: to.tracks) {
            if (toTrack.id_ == fromTrack.id_) {
                toTrack.reader = fromTrack.reader;
                break;
            }
        }
    }
}

void JuceMixPlayer::_createFileReadersAndTotalDuration() {
    for (MixerTrack& track: mixerData.tracks) {
        juce::File file(track.path);
        track.reader.reset(formatManager.createReaderFor(file));
        if (!track.reader) {
            _onErrorNotify("unable to read " + track.path);
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

void JuceMixPlayer::_loadRepeatedTrack(int block,
                                       int blockDuration,
                                       juce::AudioBuffer<float>& output,
                                       float offset,
                                       float repeatInterval,
                                       juce::AudioBuffer<float>* track)
{
    const int numChannels = output.getNumChannels();
    const int outputLength = output.getNumSamples();              // blockDuration * sampleRate
    const int trackLength = track->getNumSamples();
    const int blockStart = block * blockDuration * sampleRate;  // first sample of this block in the full timeline

    const int offsetSamples   = static_cast<int>(offset * sampleRate);
    const int intervalSamples = static_cast<int>(repeatInterval * sampleRate);

    for (int repeatIndex = 0; ; ++repeatIndex)
    {
        // absolute sample where this repeat begins
        int repeatStartAbs = offsetSamples + repeatIndex * intervalSamples;
        // if the start is beyond the end of this block, we're done
        if (repeatStartAbs >= blockStart + outputLength)
            break;

        // if the end of this track-play occurs before this block starts, skip it
        if (repeatStartAbs + trackLength <= blockStart)
            continue;

        // local position in the block buffer
        int writePos = repeatStartAbs - blockStart;
        // if it's negative, we'll start reading from inside the track
        int trackReadPos = writePos < 0 ? -writePos : 0;
        // clamp to the block
        int samplesToCopy = std::min(trackLength - trackReadPos, outputLength - std::max(writePos, 0));

        for (int channel = 0; channel < numChannels; ++channel) {
            int trackChannel = (channel < track->getNumChannels() ? channel : 0);
            output.addFrom(channel, std::max(writePos, 0) /*destStartSample*/, *track, trackChannel,/*srcStartSample=*/ trackReadPos, samplesToCopy, 1.0f);
        }
    }
}

void JuceMixPlayer::_loadAudioBlockSafe(int block, bool reset, std::function<void()> completion) {
    int taskQueueIndex = reset ? ++this->taskQueueIndex : this->taskQueueIndex;
    heavyTaskQueue.async([&, taskQueueIndex, block, reset, completion] {
        if (reset) {
            loadingBlocks.clear();
            loadedBlocks.clear();
            playBuffer.clear();
        }
        _loadAudioBlock(block, taskQueueIndex);
        taskQueue.async([&, completion, taskQueueIndex] {
            if (taskQueueIndex == this->taskQueueIndex) {
                completion();
            }
        });
    });
}

void JuceMixPlayer::_loadAudioBlock(int block, int taskQueueIndex) {
    if (taskQueueIndex != this->taskQueueIndex) return;

    const float destStartSample = block * blockDuration * sampleRate;
    if (destStartSample >= playBuffer.getNumSamples()) {
        return;
    }

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

    // clear the result block
    int start = (int)(block * blockDuration * sampleRate);
    int num = (int)(blockDuration * sampleRate);
    if (start + num <= playBuffer.getNumSamples()) {
        playBuffer.clear(start, num);
    }

    int sampleCount = blockDuration * sampleRate;

    for (MixerTrack& track: mixerData.tracks) {
        if (taskQueueIndex != this->taskQueueIndex) return;

        if (!track.enabled) {
            continue;
        }
        if (!track.reader) {
            _onErrorNotify("reader not found for " + track.path);
            continue;
        }

        // clear block buffer
        tempBuffer.clear();

        // mix audio from block buffer
        if (destStartSample + sampleCount > playBuffer.getNumSamples()) {
            sampleCount = playBuffer.getNumSamples() - destStartSample;
        }

        if (track.repeat) {
            int sampleCount = (int)track.reader->lengthInSamples;
            std::shared_ptr<juce::AudioBuffer<float>> buff;
            if (repetedBufferCache.find(track.path) == repetedBufferCache.end()) {
                buff.reset(new juce::AudioBuffer<float>(2, sampleCount));
                track.reader->read(buff.get(), 0, sampleCount, 0, true, true);
                if (taskQueueIndex != this->taskQueueIndex) return;
                repetedBufferCache[track.path] = buff;
            } else {
                buff = repetedBufferCache.at(track.path);
            }
            _loadRepeatedTrack(block, blockDuration, tempBuffer, track.offset, track.repeatInterval, buff.get());
        } else {
            auto res = _calculateBlockToRead(block, track);
            if (!res.has_value()) {
                continue;
            }

            float dstStart = std::get<0>(res.value());
            float numSamples = std::get<1>(res.value());
            float readStart = std::get<2>(res.value());

            // read data into block buffer
            const bool success = track.reader->read(&tempBuffer, dstStart, numSamples, readStart, true, true);
            if (taskQueueIndex != this->taskQueueIndex) return;
            if (!success) {
                std::string err = "Read operation was not success for: " + track.path;
                _onErrorNotify(err);
            }
            auto listener = trackLoadListener;
            if (listener) {
                listener(track.id_,
                         tempBuffer,
                         sampleRate);
            }
        }

        for (int i=0; i<2; i++) {
            playBuffer.addFrom(i, destStartSample, tempBuffer, i, 0, sampleCount, track.volume);
        }
    }

    if (taskQueueIndex != this->taskQueueIndex) return;
    auto listener = mergeReadyListener;
    if (listener) {
        for (int i=0; i<2; i++) {
            tempBuffer.copyFrom(i, 0, playBuffer, i, destStartSample, sampleCount);
        }
        bool shouldMerge = listener(tempBuffer, sampleRate);
        if (shouldMerge) {
            for (int i=0; i<2; i++) {
                playBuffer.addFrom(i, destStartSample, tempBuffer, i, 0, sampleCount, 1.0f);
            }
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

void JuceMixPlayer::exportToFile(const char* outputFile, std::function<void(const char*)> completion) {
    float duration = getDuration();
    if (duration <= 0) {
        completion("Duration is 0");
        return;
    }
    if (_isPlaying || _isRecording) {
        completion("Export not supported while playing/recording");
        return;
    }
    heavyTaskQueue.async([&, outputFile, completion]{
        _isExporting = true;
        int total = getDuration() / blockDuration;
        for (int i=0; i<total; i++) {
            _loadAudioBlock(i, taskQueueIndex);
        }
        _isExporting = false;

        int targetSampleRate = settings.sampleRate;
        juce::File file(outputFile);
        file.deleteFile();
        juce::FileOutputStream* outputStream = new juce::FileOutputStream(file);
        std::shared_ptr<juce::AudioFormat> audioFormat;
        std::shared_ptr<juce::AudioFormatWriter> writer;
        if (juce::String(outputFile).toLowerCase().endsWith("wav")) {
            audioFormat.reset(new juce::WavAudioFormat());
        } else if (juce::String(outputFile).toLowerCase().endsWith("flac")) {
            audioFormat.reset(new juce::FlacAudioFormat());
        } else {
            completion("Failed to export, unsupported file extension");
            return;
        }
        writer.reset(audioFormat->createWriterFor(outputStream, targetSampleRate, 1, 16, {}, 0));
        bool success = writer->writeFromAudioSampleBuffer(playBuffer, 0, playBuffer.getNumSamples());
        completion(success ? "" : "Failed to export");
    });
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
        _createWriterForRecorder();
    });
}

void JuceMixPlayer::startRecorder() {
    if (_isRecording) return;
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        if (!_isRecorderPrepared) {
            if (onRecErrorCallback) onRecErrorCallback(this, "Failed to start recording, prepare not called");
            _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
            return;
        }

        bool success = setAudioSessionRecord();
        if (!success) {
            if (onRecErrorCallback) onRecErrorCallback(this, "Failed to start system audio session");
            _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
            return;
        }

        deviceManagerSavedState = deviceManager->createStateXml();
        deviceManager->closeAudioDevice();

        deviceCallbackTime1 = _getEpochTime();

        deviceManager->initialise(1, 2, deviceManagerSavedState.get(), true, {}, nullptr);

        deviceCallbackTime2 = _getEpochTime();

        success = setAudioSessionRecord();
        if (!success) {
            if (onRecErrorCallback) onRecErrorCallback(this, "Failed to start system audio session");
            _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
            return;
        }

        taskQueue.async([&]{
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            _startProgressTimer();
            _onRecStateUpdateNotify(JuceMixPlayerRecState::RECORDING);

            if (settings.recBgPlayback) {
                _playInternal();
                playBufferTime = _getEpochTime();
            }
            _isRecording = true;
        });
    });
}

void JuceMixPlayer::stopRecorder() {
    if (!_isRecording) return;
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        if (_isRecording) {
            this->outputLatencyInSamples = deviceManager->getCurrentAudioDevice()->getOutputLatencyInSamples();
            PRINT("getDeviceLatencyInfo: " << getDeviceLatencyInfo());
            stop();
            _stopProgressTimer();
            _isRecording = false;
            _finishRecording();
            deviceManagerSavedState = deviceManager->createStateXml();
            deviceManager->closeAudioDevice();
            deviceManager->initialise(0, 2, deviceManagerSavedState.get(), true, {}, nullptr);
            setAudioSessionPlay();
        }
    });
}

void JuceMixPlayer::_createWriterForRecorder() {
    PRINT("recordPath: " << recordPath);
    _resetRecorder();
    if (deviceSampleRate == 0) {
        if (onRecErrorCallback) onRecErrorCallback(this, "deviceSampleRate is 0");
        _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
        return;
    }

    recordBuffer1.setSize(1, recordBufferDuration * 1.5 * deviceSampleRate);
    recordBuffer2.setSize(1, recordBufferDuration * 1.5 * deviceSampleRate);

    int targetSampleRate = settings.sampleRate;
    juce::File outputFile(recordPath);
    outputFile.deleteFile();
    juce::FileOutputStream* outputStream = new juce::FileOutputStream(outputFile);
    if (juce::String(recordPath).toLowerCase().endsWith("wav")) {
        recAudioFormat.reset(new juce::WavAudioFormat());
    } else if (juce::String(recordPath).toLowerCase().endsWith("flac")) {
        recAudioFormat.reset(new juce::FlacAudioFormat());
    } else {
        if (onRecErrorCallback) onRecErrorCallback(this, "unsupported file extension");
        _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
        return;
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

void JuceMixPlayer::flushRecordBufferToFile(juce::AudioBuffer<float>& buffer, int sampleCount) {
    if (!recWriter) {
        if (onRecErrorCallback)
            onRecErrorCallback(this, "Failed to write file, writer not created");
        _onRecStateUpdateNotify(JuceMixPlayerRecState::ERROR);
        return;
    }
    if (sampleCount <= 0) {
        return;
    }
    PRINT("flushRecordBufferToFile: " << sampleCount / deviceSampleRate);
    bool success = false;

    const int targetSampleRate = settings.sampleRate;
    const int numInputSamples = sampleCount;

    if (deviceSampleRate != targetSampleRate) {
        PRINT("flushRecordBufferToFile: REPSAMPLING =====");
        const double ratio = static_cast<double>(targetSampleRate) / deviceSampleRate;
        const double actualRatio = static_cast<double>(deviceSampleRate) / targetSampleRate;
        const int numChannels = buffer.getNumChannels();
        const int numOutputSamples = static_cast<int>(std::ceil(numInputSamples * ratio));

        // Create upsampled buffer
        juce::AudioBuffer<float> upsampledBuffer(numChannels, numOutputSamples);

        // Resample each channel
        for (int ch = 0; ch < numChannels; ++ch) {
            const float* inputData = buffer.getReadPointer(ch);
            float* outputData = upsampledBuffer.getWritePointer(ch);

            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process(actualRatio,
                                 inputData,
                                 outputData,
                                 numOutputSamples,
                                 numInputSamples,
                                 0);
        }

        const double expectedDuration = static_cast<double>(numInputSamples) / deviceSampleRate;
        const double resultDuration = static_cast<double>(upsampledBuffer.getNumSamples()) / targetSampleRate;
        const double durationDifference = std::abs(expectedDuration - resultDuration);

        PRINT("- Original: " << numInputSamples << " samples @ " << deviceSampleRate << "Hz (" << expectedDuration << "s)");
        PRINT("- Resampled: " << upsampledBuffer.getNumSamples() << " samples @ " << targetSampleRate << "Hz (" << resultDuration << "s)");
        PRINT("- Duration difference: " << (durationDifference * 1000.0) << "ms");
        jassert(durationDifference < (1.0 / targetSampleRate) &&
                "Resampling duration mismatch exceeds 1 sample tolerance");
        success = recWriter->writeFromAudioSampleBuffer(upsampledBuffer, 0, upsampledBuffer.getNumSamples());
    } else {
        success = recWriter->writeFromAudioSampleBuffer(buffer, 0, numInputSamples);
    }

    // Flush and handle errors
    success = success && recWriter->flush();

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

// MARK: adding custom filters pass
void JuceMixPlayer::setTrackLoadListener(std::function<bool(std::string trackId,
                                                            juce::AudioBuffer<float>& buffer,
                                                            int sampleRate)> closure) {
    this->trackLoadListener = closure;
}

void JuceMixPlayer::setMergeReadyListener(std::function<bool(juce::AudioBuffer<float>& buffer,
                                                             int sampleRate)> closure) {
    this->mergeReadyListener = closure;
}

// MARK: Device management
void JuceMixPlayer::notifyDeviceUpdates() {
    MixerDeviceList list;

    juce::AudioIODeviceType* audioDeviceType = deviceManager->getCurrentDeviceTypeObject();
    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();

    if (audioDeviceType) {
        juce::AudioIODevice* currentDevice = deviceManager->getCurrentAudioDevice();
        if (currentDevice != nullptr) {
            // Current Input Device Info
            MixerDevice inputDev;
            inputDev.name = currentDevice->getName().toStdString();
            inputDev.isInput = true;
            inputDev.isSelected = (setup.inputDeviceName == currentDevice->getName());

            // Get input channels
            juce::StringArray inputChannels = currentDevice->getInputChannelNames();
            for (const auto& ch : inputChannels)
                inputDev.inputChannelNames.push_back(ch.toStdString());

            inputDev.currentSampleRate = currentDevice->getCurrentSampleRate();
            for (auto rate : currentDevice->getAvailableSampleRates())
                inputDev.availableSampleRates.push_back(rate);
            inputDev.deviceType = audioDeviceType->getTypeName().toStdString();
            list.devices.push_back(inputDev);

            // Current Output Device Info
            MixerDevice outputDev;
            outputDev.name = currentDevice->getName().toStdString();
            outputDev.isInput = false;
            outputDev.isSelected = (setup.outputDeviceName == currentDevice->getName());

            // Get output channels
            juce::StringArray outputChannels = currentDevice->getOutputChannelNames();
            for (const auto& ch : outputChannels)
                outputDev.outputChannelNames.push_back(ch.toStdString());

            outputDev.currentSampleRate = currentDevice->getCurrentSampleRate();
            outputDev.availableSampleRates = inputDev.availableSampleRates; // Same device
            outputDev.deviceType = inputDev.deviceType;
            list.devices.push_back(outputDev);
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
    //    std::string json_(json);
    //    taskQueue.async([&, json_]{
    //        try {
    //            MixerDeviceList list = MixerDeviceList::decode(json_);
    //            if (!(deviceList == list)) {
    //                deviceList = list;
    //
    //                MixerDevice inp;
    //                MixerDevice out;
    //                for (auto& dev: list.devices) {
    //                    if (dev.isSelected && dev.isInput && inp.name == "") {
    //                        inp = dev;
    //                    }
    //                    if (dev.isSelected && !dev.isInput && out.name == "") {
    //                        out = dev;
    //                    }
    //                }
    //
    //                bool hasChanges = inp.name != "" || out.name != "";
    //
    //                if (!hasChanges) {
    //                    PRINT("setUpdatedDevices: No selected device found");
    //                    return;
    //                }
    //
    //                PRINT("setUpdatedDevices: selected inp: " << inp.name << ", out: " << out.name);
    //
    //                juce::MessageManager::getInstanceWithoutCreating()->callAsync([&, inp, out]{
    //                    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
    //                    if (inp.name != "") setup.inputDeviceName = juce::String(inp.name);
    //                    if (out.name != "") setup.outputDeviceName = juce::String(out.name);
    //                    bool treatAsChosenDevice = true;
    //                    juce::String err = deviceManager->setAudioDeviceSetup(setup, treatAsChosenDevice);
    //                    if (err.isNotEmpty()) {
    //                        _onErrorNotify(err.toStdString());
    //                    }
    //                    notifyDeviceUpdates();
    //                });
    //            } else {
    //                PRINT("setUpdatedDevices: Same device data! ignoring");
    //            }
    //        } catch (const std::exception& e) {
    //            _onErrorNotify(std::string(e.what()));
    //            notifyDeviceUpdates();
    //        }
    //    });
}

void JuceMixPlayer::setDefaultSampleRate() {
    //    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
    //    setup.sampleRate = settings.sampleRate;
    //    bool treatAsChosenDevice = true;
    //    deviceManager->setAudioDeviceSetup(setup, treatAsChosenDevice);
}

long JuceMixPlayer::_getEpochTime() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return duration.count();
}

const char* JuceMixPlayer::getDeviceLatencyInfo() {
    DeviceLaencyInfo info;
    info.sampleRate = deviceSampleRate;
    info.bufferLatency = samplesPerBlockExpected * 1000 / deviceSampleRate;
    info.outputLatency = outputLatencyInSamples * 1000 / deviceSampleRate;
    info.inputLatency = inputLatencyInSamples * 1000 / deviceSampleRate;
    info.deviceCallbackTime1 = deviceCallbackTime1;
    info.deviceCallbackTime2 = deviceCallbackTime2;
    info.playBufferTime = playBufferTime;
    info.timeDiff = info.bufferLatency * 5.6;

    nlohmann::json j = info;
    return returnCopyCharDelete(j.dump(4));
}

// MARK: AudioIODeviceCallback
void JuceMixPlayer::audioDeviceAboutToStart(juce::AudioIODevice *device) {
    if (deviceCallbackTime1 > 99999) {
        deviceCallbackTime1 = _getEpochTime() - deviceCallbackTime1;
    }
    this->deviceSampleRate = device->getCurrentSampleRate();
    this->samplesPerBlockExpected = device->getCurrentBufferSizeSamples();

    PRINT("audioDeviceAboutToStart" <<
          ", bufferSizeSamples: " << samplesPerBlockExpected <<
          ", deviceSampleRate: " << deviceSampleRate
          );
}

void JuceMixPlayer::audioDeviceIOCallbackWithContext(const float *const *inputChannelData,
                                                     int numInputChannels,
                                                     float *const *outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext &context) {
    if (deviceCallbackTime2 > 99999) {
        deviceCallbackTime2 = _getEpochTime() - deviceCallbackTime2;
    }

    bool enterPlayerBlock = !_isSeeking && _isPlayingInternal && _isPlaying && numOutputChannels > 0;

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

    if (enterPlayerBlock) {
        float speedRatio = sampleRate/deviceSampleRate;
        float readCount = (float)numSamples * speedRatio;

        if (playHeadIndex + readCount > playBuffer.getNumSamples()) {
            if (playBuffer.getNumSamples() == 0) {
                _onStateUpdateNotify(JuceMixPlayerState::IDLE);
            } else {
                _onProgressNotify(1);
                _onStateUpdateNotify(JuceMixPlayerState::COMPLETED);

                if (_isRecording && settings.stopRecOnPlaybackComplete) {
                    stopRecorder();
                }
            }

            if (!_isRecording && settings.loop) {
                _isPlaying = false;
                playHeadIndex = 0;
                play();
            } else {
                _pauseInternal(false);
            }
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
        _loadAudioBlockSafe((getCurrentTime()/blockDuration)+1, false, []{});
    } else {
        for (int ch=0; ch<numOutputChannels; ch++) {
            juce::zeromem(outputChannelData[ch], (size_t) numSamples * sizeof (float));
        }
    }

    if (_isRecording && numInputChannels > 0 && settings.enableMicMonitoring) {
        juce::AudioBuffer<float> outData(outputChannelData, numOutputChannels, numSamples);
        for (int ch=0; ch<numOutputChannels; ch++) {
            outData.addFrom(ch, 0, inputChannelData[0], numSamples);
        }
    }

    if (enterPlayerBlock && playBufferTime > 99999) {
        playBufferTime = _getEpochTime() - playBufferTime;
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
        if (!_isSeeking && _isPlayingInternal && _isPlaying && playBuffer.getNumSamples() > 0) {
            _onProgressNotify((float)playHeadIndex / (float)playBuffer.getNumSamples());
        }
        if (_isRecording) {
            if (onRecProgressCallback) {
                onRecProgressCallback(this, (float)recordTimerIndex / (float)deviceSampleRate);
            }
            if (onRecLevelCallback) {
                const float level = inputLevelMeter->getCurrentLevel();
                const float levelInDb = juce::Decibels::gainToDecibels(level);
                onRecLevelCallback(this, levelInDb);
            }
        }
    });
}

void JuceMixPlayer::changeListenerCallback(juce::ChangeBroadcaster* source) {
    PRINT("changeListenerCallback");
    notifyDeviceUpdates();
}
