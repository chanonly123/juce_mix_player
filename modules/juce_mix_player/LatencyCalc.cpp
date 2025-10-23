#include "LatencyCalc.h"

LatencyCalc::LatencyCalc()
{
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        sharedDeviceManager()->addAudioCallback(this);
        PRINT("LatencyCalc initialized");
    });
}

void LatencyCalc::start(JuceMixPlayerCallbackString callback)
{
    if (_isLatencyCalc) {
        return;
    }
    _isLatencyCalc = true;
    onCalucateLatencyCallback = callback;
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        
        MixerSettings settings;
        bool success = setAudioSessionRecord(settings);
        
        if (!success) {
            if (onCalucateLatencyCallback)
                onCalucateLatencyCallback(this, "Failed to start system audio session");
            _isLatencyCalc = false;
            return;
        }
        
        sharedDeviceManager()->initialiseWithDefaultDevices(1, 2);
        
        taskQueue.async([&]{
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            _isLatencyCalc = true;
        });
    });
}

void LatencyCalc::stop()
{
    _isLatencyCalc = false;
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        sharedDeviceManager()->closeAudioDevice();
    });
}

void LatencyCalc::audioDeviceIOCallbackWithContext(const float *const *inputChannelData,
                                                   int numInputChannels,
                                                   float *const *outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext &context)
{
    if (_isLatencyCalc && playBufferSize > 0) {
        if (playHeadIndex + numSamples > playBufferSize) {
            playHeadIndex = 0;
        
            int recPos = findTwoTickPattern(bufferRec, tickGap);
            if (playTickPosition >=0 && recPos >=0) {
                stop();
                if (onCalucateLatencyCallback != nullptr) {
                    int latencySamples = float(recPos - playTickPosition) / float(playBufferSize/1000);
                    onCalucateLatencyCallback(this, returnCopyCharDelete(std::to_string(latencySamples)));
                }
            }
        }
        
        for (int ch=0; ch<numOutputChannels; ch++) {
            memcpy(outputChannelData[ch],
                   bufferPlay.getReadPointer(ch, playHeadIndex),
                   (size_t) numSamples * sizeof (float));
        }
        
        for (int ch=0; ch<numInputChannels; ch++) {
            memcpy(bufferRec.getWritePointer(ch, playHeadIndex),
                   inputChannelData[ch],
                   (size_t) numSamples * sizeof (float));
        }
        
        playHeadIndex += numSamples;
    }
}

void LatencyCalc::audioDeviceError(const juce::String &errorMessage)
{
    PRINT("audioDeviceError: " << errorMessage);
}

void LatencyCalc::audioDeviceStopped()
{
    
}

void LatencyCalc::audioDeviceAboutToStart(juce::AudioIODevice *device) {
    playBufferSize = device->getCurrentSampleRate();
    tickGap = playBufferSize / 4;
    playTickPosition = playBufferSize / 4;
    
    if (playBufferSize > 0) {
        bufferPlay.setSize(2, playBufferSize * 2);
        bufferPlay.clear();
        bufferRec.setSize(1, playBufferSize * 2);
        bufferRec.clear();
        
        playHeadIndex = 0;
        
        // set beep at 0.25 and 0.5 position
        for (int i=0; i<bufferPlay.getNumChannels(); i++) {
            bufferPlay.setSample(i, playBufferSize / 4, 1.0f);
            bufferPlay.setSample(i, playBufferSize / 2, 1.0f);
        }
    }
}

void LatencyCalc::dispose() {
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        PRINT("LatencyCalc::dispose");
        sharedDeviceManager()->removeAudioCallback(this);
        std::thread thread([&]{
            taskQueue.stopQueue();
            juce::Thread::sleep(5000);
            delete this;
        });
        thread.detach();
    });
}

LatencyCalc::~LatencyCalc()
{
    PRINT("~LatencyCalc");
}

int LatencyCalc::findTwoTickPattern(juce::AudioBuffer<float> buff, int tickGap)
{
    int size = buff.getNumSamples();
    const float minValue = 0.3;
    int high1 = 0;
    int high2 = high1 + tickGap;
    int tickMidTolleranceSamples = 100;
    int foundPos = -1;
        
    float foundPeak = buff.getMagnitude(0, 0, size);
    if (foundPeak <= minValue) {
        float gain = (minValue + 0.2) / foundPeak;
        buff.applyGain(gain);
    }
    float finalPeak = buff.getMagnitude(0, 0, size);
    
    std::cout << "========" << foundPos << std::endl;
    while (high1 < size && high2 < size) {
        if (
            buff.getMagnitude(0, high1, 2) > minValue
            && buff.getMagnitude(0, high2, 2) > minValue
        ) {
            float midValue = buff.getMagnitude(0,
                                               high1 + tickMidTolleranceSamples,
                                               tickGap - (tickMidTolleranceSamples * 2));
            std::cout << "midValue: " << midValue << std::endl;
            if (midValue < minValue) {
                foundPos = high1;
                break;
            }
        }
        high1++;
        high2++;
    }
    std::cout << "foundPeak: " << foundPeak << " => " << finalPeak << std::endl;
    std::cout << "foundPos: " << foundPos << std::endl;
    return foundPos;
}
