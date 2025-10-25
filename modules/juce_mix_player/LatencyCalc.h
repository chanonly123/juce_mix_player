#pragma once

#include "OsExtras.h"
#include "JuceMixPlayer.h"

class LatencyCalc : public juce::AudioIODeviceCallback {
    
private:
    
    JuceMixPlayerCallbackString onCalucateLatencyCallback = nullptr;
    TaskQueue taskQueue;
    std::unique_ptr<juce::AudioDeviceManager::AudioDeviceSetup> setup;
    const float tickDurationSeconds = 0.03f;
    int picCount = 0;
    
    int tickGap = 0;
    juce::AudioBuffer<float> bufferRec;
    juce::AudioBuffer<float> bufferPlay;
    
    int playBufferSize = 0;
    bool _isLatencyCalc = false;
    int playHeadIndex = 0;
    int playTickPosition = -1;
        
public:
    
    LatencyCalc();
    
    ~LatencyCalc();
    
    void dispose();
    
    void start(JuceMixPlayerCallbackString callback);
    
    void stop();
    
    static void generateVeryShortBeep(juce::AudioBuffer<float>& buffer,
                                      int sampleRate,
                                      int fromIndex,
                                      float tickDurationSeconds);
    
    static int findTwoTickPattern(juce::AudioBuffer<float>& buff,
                                  int sampleRate,
                                  int tickGap,
                                  float tickDurationSeconds);
    
    void createImageAsync();
    
    static void createImageFile(const juce::AudioBuffer<float>& buffer,
                         const juce::File& file);

    // juce::AudioIODeviceCallback
    
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext &context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
    
    void audioDeviceError(const juce::String &errorMessage) override;
    
    void audioDeviceStopped() override;

};
