#pragma once

#include "nlohmann/json.hpp"
#include "Logger.h"
#include "TaskQueue.h"
#include "Models.h"
#include "Models.h"
#include <iostream>
#include <tuple>

class JuceMixPlayer : private juce::Timer, public juce::AudioIODeviceCallback, public juce::ChangeListener
{
private:

    inline static juce::AudioDeviceManager* deviceManager;

    juce::CriticalSection lock;
    TaskQueue heavyTaskQueue;
    int taskQueueIndex = 0;
    TaskQueue taskQueue;
    TaskQueue recWriteTaskQueue;

    std::unique_ptr<juce::XmlElement> deviceManagerSavedState;

    MixerDeviceList deviceList;

    MixerSettings settings;

    juce::AudioFormatManager formatManager;
    juce::LinearInterpolator interpolator[2];

    // MARK: Playing
    JuceMixPlayerState currentState = JuceMixPlayerState::IDLE;

    MixerData mixerData;
    bool _isPlaying = false;
    bool _isPlayingInternal = false;
    bool _isSeeking = false;
    bool _isExporting = false;
    int playHeadIndex = 0;
    juce::AudioBuffer<float> playBuffer;
    std::unordered_map<std::string, std::shared_ptr<juce::AudioBuffer<float>>> repetedBufferCache;

    // external audio filter callbacks
    std::function<bool(std::string trackId,
                       juce::AudioBuffer<float>& buffer,
                       int sampleRate)> trackLoadListener;

    std::function<bool(juce::AudioBuffer<float>& buffer,
                       int sampleRate)> mergeReadyListener;

    // MARK: Recording

    JuceMixPlayerRecState currentRecState = JuceMixPlayerRecState::IDLE;

    bool _isRecording = false;
    bool _isRecorderPrepared = false;
    int recordHeadIndex = 0;
    int recordTimerIndex = 0;
    int recordBufferSelect = 0;
    juce::AudioBuffer<float> recordBuffer1;
    juce::AudioBuffer<float> recordBuffer2;
    int recordBufferDuration = 10; // seconds
    std::shared_ptr<juce::AudioFormat> recAudioFormat;
    std::shared_ptr<juce::AudioFormatWriter> recWriter;
    std::string recordPath;
    juce::ReferenceCountedObjectPtr<juce::AudioDeviceManager::LevelMeter> inputLevelMeter;

    // loading buffer into chunks
    std::unordered_set<int> loadingBlocks;
    std::unordered_set<int> loadedBlocks;
    const float blockDuration = 5; // second
    const float sampleRate = 48000;

    // latency related
    float deviceSampleRate = -1;
    int samplesPerBlockExpected = -1;
    int outputLatencyInSamples = -1;
    int inputLatencyInSamples = -1;
    long deviceCallbackTime1 = -1;
    long deviceCallbackTime2 = -1;
    long playBufferTime = -1;

    void _prepare();

    void _playInternal();

    void _pauseInternal(bool stop);

    /// create reader for files
    void _createFileReadersAndTotalDuration();

    void _loadRepeatedTrack(int block,
                            int blockDuration,
                            juce::AudioBuffer<float>& output,
                            float offset,
                            float repeatInterval,
                            juce::AudioBuffer<float>* track);

    void _loadAudioBlockSafe(int block, bool reset, std::function<void()> completion);

    /// loads audio block for `blockDuration` and `block` number. Blocks are chunks of the audio file.
    void _loadAudioBlock(int block, int taskQueueIndex);

    void _onProgressNotify(float progress);

    void _onStateUpdateNotify(JuceMixPlayerState state);

    void _onErrorNotify(std::string error);

    std::optional<std::tuple<float, float, float>> _calculateBlockToRead(float block, MixerTrack& track);

    void _createWriterForRecorder();

    void flushRecordBufferToFile(juce::AudioBuffer<float>& buffer, int sampleCount);

    void _onRecStateUpdateNotify(JuceMixPlayerRecState state);

    void _finishRecording();

    void _resetRecorder();

    void _startProgressTimer();

    void _stopProgressTimer();

    void notifyDeviceUpdates();

    void setDefaultSampleRate();

    void _resetPlayBufferBlocks();

    void _copyReaders(const MixerData& from, MixerData& to);

    // returns epoch time in millis
    long _getEpochTime();
    
public:

    JuceMixPlayerCallbackFloat onProgressCallback = nullptr;
    JuceMixPlayerCallbackString onStateUpdateCallback = nullptr;
    JuceMixPlayerCallbackString onErrorCallback = nullptr;

    JuceMixPlayerCallbackFloat onRecLevelCallback = nullptr;
    JuceMixPlayerCallbackFloat onRecProgressCallback = nullptr;
    JuceMixPlayerCallbackString onRecStateUpdateCallback = nullptr;
    JuceMixPlayerCallbackString onRecErrorCallback = nullptr;

    JuceMixPlayerCallbackString onDeviceUpdateCallback = nullptr;

    JuceMixPlayer();

    ~JuceMixPlayer();

    void dispose();

    void play();

    void pause();

    void stop();

    void togglePlayPause();

    void setJson(const char* json);

    void setSettings(const char* json);

    /// value range 0 to 1
    void seek(float value);

    // get curent time in seconds
    float getCurrentTime();

    // get total duration in seconds
    float getDuration();

    int isPlaying();

    std::string getCurrentState();

    void exportToFile(const char* outputFile, std::function<void(const char*)> completion);

    // MARK: Recorder
    void prepareRecorder(const char* file);

    void startRecorder();

    void stopRecorder();

    // MARK: adding custom filters pass
    void setTrackLoadListener(std::function<bool(std::string trackId,
                                                 juce::AudioBuffer<float>& buffer,
                                                 int sampleRate)> closure);

    void setMergeReadyListener(std::function<bool(juce::AudioBuffer<float>& buffer,
                                                  int sampleRate)> closure);

    void resetPlayBuffer();

    // MARK: device management
    void setUpdatedDevices(const char* json);

    const char* getDeviceLatencyInfo();

    // MARK: juce::AudioIODeviceCallback
    void audioDeviceAboutToStart(juce::AudioIODevice *device) override;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext &context) override;

    void audioDeviceStopped() override;

    void audioDeviceError(const juce::String &errorMessage) override;

    // MARK: juce::Timer
    void timerCallback() override;

    // MARK: juce::ChangeListener

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
};
