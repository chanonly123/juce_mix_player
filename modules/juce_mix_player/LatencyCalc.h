#pragma once

#include "OsExtras.h"
#include "JuceMixPlayer.h"

struct FFiPointer
{
    void* ptr;
    unsigned long size;
};

class LatencyCalc : public juce::AudioIODeviceCallback {
    
private:
    
    JuceMixPlayerCallbackString onCalucateLatencyCallback = nullptr;
    TaskQueue taskQueue;
    std::unique_ptr<juce::AudioDeviceManager::AudioDeviceSetup> setup;
    const float tickDurationSeconds = 0.03f;
    
    juce::MemoryBlock imageBuffer;
    juce::MemoryOutputStream memStream;
    
    int tickGap = 0;
    juce::AudioBuffer<float> bufferRec;
    juce::AudioBuffer<float> bufferPlay;
    
    juce::String devSettings = "image_gen,gain_normalize,~image_gen";
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
                                  float tickDurationSeconds,
                                  bool enableGain);
    
    void createImageAsync();
    
    juce::String setDevSettings(juce::String option);
    
    void fillImageBuffer(const juce::AudioBuffer<float>& buffer);
    
    FFiPointer getImageBufferPointer();
    
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
