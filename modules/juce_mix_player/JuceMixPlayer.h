#pragma once

#include "nlohmann/json.hpp"
#include "Logger.h"
#include "TaskQueue.h"
#include "Models.h"
#include "Models.h"
#include <iostream>
#include <tuple>

typedef void (*JuceMixPlayerCallbackFloat)(void*, float);
typedef void (*JuceMixPlayerCallbackString)(void*, const char*);

enum JuceMixPlayerState
{
    IDLE, READY, PLAYING, PAUSED, STOPPED, ERROR, COMPLETED
};

NLOHMANN_JSON_SERIALIZE_ENUM(JuceMixPlayerState,{
    {IDLE, "IDLE"},
    {READY, "READY"},
    {PLAYING, "PLAYING"},
    {PAUSED, "PAUSED"},
    {STOPPED, "STOPPED"},
    {ERROR, "ERROR"},
    {COMPLETED, "COMPLETED"},
});

std::string JuceMixPlayerState_toString(JuceMixPlayerState state);
JuceMixPlayerState JuceMixPlayerState_make(std::string state);

class JuceMixPlayer : private juce::Timer, public juce::AudioIODeviceCallback
{
private:
    juce::CriticalSection lock;
    TaskQueue taskQueue;
    TaskQueue recWriteTaskQueue;
    int samplesPerBlockExpected = 0;
    float deviceSampleRate = 0;
    float progressUpdateInterval = 0.05;
    juce::AudioFormatManager formatManager;
    juce::LinearInterpolator interpolator[2];
    juce::AudioDeviceManager* deviceManager;

    JuceMixPlayerState currentState = IDLE;

    JuceMixPlayerCallbackFloat onProgressCallback = nullptr;
    JuceMixPlayerCallbackString onStateUpdateCallback = nullptr;
    JuceMixPlayerCallbackString onErrorCallback = nullptr;

    bool _isPlaying = false;
    bool _isRecording = false;

    juce::AudioBuffer<float> playBuffer;

    juce::AudioBuffer<float> recordBuffer;
    std::shared_ptr<juce::AudioFormat> recAudioFormat;
    std::shared_ptr<juce::AudioFormatWriter> recWriter;
    std::string recordPath;

    MixerData mixerData;

    int playHeadIndex = 0;
    int recordHeadIndex = 0;
    int recordHeadWriteIndex = 0;

    std::unordered_set<int> loadingBlocks;
    std::unordered_set<int> loadedBlocks;
    const float blockDuration = 10; // second
    const float sampleRate = 48000;

    void prepare();

    void _play();

    void _pause(bool stop);

    /// create reader for files
    void _createFileReadersAndTotalDuration();

    /// loads audio block for `blockDuration` and `block` number. Blocks are chunks of the audio file.
    void _loadAudioBlock(int block);

    void _loadRepeatedTracks();

    void _onProgressNotify(float progress);

    void _onStateUpdateNotify(JuceMixPlayerState state);

    void _onErrorNotify(std::string error);

    std::optional<std::tuple<float, float, float>> _calculateBlockToRead(float block, MixerTrack& track);

    void loadCompleteBuffer(juce::AudioBuffer<float>& buffer, bool takeRepeteTracks, bool takeNonRepeteTracks);

    void createWriterForRecorder();

    void writeRecChunk();

    bool writeBufferToFile(juce::AudioBuffer<float>& buffer,
                           const juce::String& outputFilePath,
                           double targetSampleRate,
                           int numChannels,
                           int sampleCount,
                           std::string format);
public:
    JuceMixPlayer(int record, int play);

    ~JuceMixPlayer();

    void dispose();

    void play();

    void pause();

    void stop();

    void togglePlayPause();

    void setJson(const char* json);

    /// value range 0 to 1
    void seek(float value);

    void startRecorder(const char* file);

    void stopRecorder();

    void onProgress(JuceMixPlayerCallbackFloat callback);

    void onStateUpdate(JuceMixPlayerCallbackString callback);

    void onError(JuceMixPlayerCallbackString callback);

    // get curent time in seconds
    float getCurrentTime();

    // set timer fire interval, in seconds
    void setProgressUpdateInterval(float time);

    // get total duration in seconds
    float getDuration();

    int isPlaying();

    std::string getCurrentState();

    // juce::AudioIODeviceCallback
    void audioDeviceAboutToStart(juce::AudioIODevice *device) override;

    void audioDeviceIOCallbackWithContext(const float *const *inputChannelData, int numInputChannels, float *const *outputChannelData, int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext &context) override;

    void audioDeviceStopped() override;

    void audioDeviceError(const juce::String &errorMessage) override;

    // juce::Timer
    void timerCallback() override;
};
