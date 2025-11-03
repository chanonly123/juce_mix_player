#include "LatencyCalc.h"

LatencyCalc::LatencyCalc()
{
    setup.reset(new juce::AudioDeviceManager::AudioDeviceSetup());
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        sharedDeviceManager()->addAudioCallback(this);
        PRINT("LatencyCalc initialized");
    });
}

void LatencyCalc::start(JuceMixPlayerCallbackString callback)
{
    onCalucateLatencyCallback = callback;
    if (_isLatencyCalc) {
        return;
    }
    _isLatencyCalc = true;
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        
        MixerSettings settings;
        settings.dissallowBluetoothMic = true;
        
        bool success = setAudioSessionRecord(settings);
        if (!success) {
            if (onCalucateLatencyCallback)
                onCalucateLatencyCallback(this, "Failed to start system audio session");
            _isLatencyCalc = false;
            return;
        }
        
        setup->sampleRate = 48000;
        sharedDeviceManager()->initialise(1, 2, nullptr, true, {}, setup.get());
        
        success = setAudioSessionRecord(settings);
        if (!success) {
            if (onCalucateLatencyCallback)
                onCalucateLatencyCallback(this, "Failed to start system audio session");
            _isLatencyCalc = false;
            return;
        }
        
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
        if (onCalucateLatencyCallback)
            onCalucateLatencyCallback(this, "Stopped");
    });
}

juce::String LatencyCalc::setDevSettings(juce::String option)
{
    if (option.isEmpty()) {
        return devSettings;
    }
    if (option.startsWith("~")) {
        if (!devSettings.contains(option)) {
            devSettings = devSettings + "," + option;
        }
    } else {
        if (devSettings.contains("~" + option)) {
            devSettings = devSettings.replace(",~" + option, "");
        }
    }
    PRINT("devSettings: " << devSettings);
    return devSettings;
}

void LatencyCalc::audioDeviceAboutToStart(juce::AudioIODevice *device) {
    playBufferSize = device->getCurrentSampleRate();
    tickGap = playBufferSize / 4;
    playTickPosition = playBufferSize / 4;
    
    PRINT("audioDeviceAboutToStart: " << playBufferSize);
    
    if (playBufferSize > 0) {
        bufferPlay.setSize(2, playBufferSize);
        bufferPlay.clear();
        bufferRec.setSize(1, playBufferSize);
        bufferRec.clear();
        
        playHeadIndex = 0;
        
        LatencyCalc::generateVeryShortBeep(bufferPlay,
                                           playBufferSize,
                                           playBufferSize / 4,
                                           tickDurationSeconds);
        LatencyCalc::generateVeryShortBeep(bufferPlay,
                                           playBufferSize,
                                           playBufferSize / 2,
                                           tickDurationSeconds);
    }
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
            
            bool enableGain = !devSettings.contains("~gain_normalize");
            int recPos = findTwoTickPattern(bufferRec,
                                            playBufferSize,
                                            tickGap,
                                            tickDurationSeconds,
                                            enableGain);
            
            if (playTickPosition >=0 && recPos >=0) {
                stop();
                if (onCalucateLatencyCallback != nullptr) {
                    int latencySamples = float(recPos - playTickPosition) / float(playBufferSize/1000);
                    onCalucateLatencyCallback(this, returnCopyCharDelete(std::to_string(latencySamples)));
                }
            }
            
            if (!devSettings.contains("~image_gen") ) {
                createImageAsync();
            }
            
            playHeadIndex = 0;
            bufferRec.clear();
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

void LatencyCalc::dispose() {
    juce::MessageManager::callAsync([&]{
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

int LatencyCalc::findTwoTickPattern(juce::AudioBuffer<float>& buff,
                                    int sampleRate,
                                    int tickGap,
                                    float tickDurationSeconds,
                                    bool enableGain)
{
    int size = buff.getNumSamples();
    const float minValue = 0.9;
    int high1 = 0;
    int high2 = high1 + tickGap;
    const int tickMidToleranceSamples = static_cast<int>(sampleRate * tickDurationSeconds);
    int foundPos = -1;
    
    float foundPeak = buff.getMagnitude(0, 0, size);
    float gain = minValue / foundPeak;
    if (enableGain) {
        buff.applyGain(gain);
    }
    
    float finalPeak = buff.getMagnitude(0, 0, size);
    float* arr = buff.getWritePointer(0);
    
    PRINT("========");
    while (high1 < size && high2 < size) {
        if (
            std::abs(arr[high1]) >= (minValue - 0.2)
            && std::abs(arr[high2]) >= (minValue - 0.2)
            ) {
                float midValue = buff.getMagnitude(0,
                                                   high1 + tickMidToleranceSamples,
                                                   tickGap - tickMidToleranceSamples * 2);
                PRINT("midValue: " << midValue);
                if (midValue < minValue - 0.2) {
                    foundPos = high1;
                    break;
                }
            }
        high1++;
        high2++;
    }
    PRINT("foundPeak: " << foundPeak << " => " << finalPeak);
    PRINT("foundPos: " << foundPos);
    return std::max(-1, foundPos - (tickMidToleranceSamples/2));
}

void LatencyCalc::generateVeryShortBeep(juce::AudioBuffer<float>& buffer,
                                        int sampleRate,
                                        int fromIndex,
                                        float tickDurationSeconds)
{
    const int numSamples = static_cast<int>(tickDurationSeconds * sampleRate);
    
    const double frequency = 1000.0;
    const double phaseInc = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    double phase = 0.0;
    
    for (int i = fromIndex; i < fromIndex + numSamples; ++i)
    {
        float sample = std::sin(phase);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            buffer.setSample(ch, i, sample);
        }
        phase += phaseInc;
    }
}

void LatencyCalc::createImageAsync()
{
    juce::AudioBuffer<float>* buff = new juce::AudioBuffer<float>(1, bufferRec.getNumSamples());
    buff->copyFrom(0, 0, bufferRec.getReadPointer(0), bufferRec.getNumSamples());
    taskQueue.async([&, buff]{
        fillImageBuffer(*buff);
        delete buff;
        onCalucateLatencyCallback(this, "image_buffer_ready");
    });
}

FFiPointer LatencyCalc::getImageBufferPointer()
{
    FFiPointer ptr;
    ptr.ptr = imageBuffer.getData();
    ptr.size = imageBuffer.getSize();
    return ptr;
}

void LatencyCalc::fillImageBuffer(const juce::AudioBuffer<float>& buffer)
{
    int width = 500;
    int height = 300;
    juce::Image waveformImage (juce::Image::RGB, width, height, true);
    juce::Graphics g (waveformImage);
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    
    auto* channelData = buffer.getReadPointer (0);
    int numSamples = buffer.getNumSamples();
    
    for (int x = 0; x < width; ++x)
    {
        int sampleIndex = juce::jmap (x, 0, width, 0, numSamples - 1);
        float sample = channelData[sampleIndex];
        float y = juce::jmap (sample, -1.0f, 1.0f, (float)height, 0.0f);
        
        // draw vertical line (simple waveform)
        g.drawLine ((float)x, (float)height / 2.0f, (float)x, y);
    }
    
    g.setColour (juce::Colours::yellow);
    g.drawFittedText (juce::String(buffer.getNumSamples()),
                      juce::Rectangle(5, 5, 100, 50) ,
                      juce::Justification(juce::Justification::Flags::topLeft),
                      1);
    
    g.drawLine ((float)width / 4, 0, (float)width / 4, 20.0f, 2.0f);
    g.drawLine ((float)width / 2, 0, (float)width / 2, 20.0f, 2.0f);
    
    // draw ruler lines
    const int split = 10;
    for (int i = 0; i < split; i++) {
        float x = float(width/split*i);
        float sec = float(buffer.getNumSamples()) / float(playBufferSize) / float(split) * i;
        g.drawLine (x, 0, x, height);
        g.drawFittedText (juce::String(sec),
                          juce::Rectangle(int(x) + 2, height-15, 50, 10) ,
                          juce::Justification(juce::Justification::Flags::topLeft),
                          1);
    }
    
    juce::PNGImageFormat format;
    memStream.reset();
    format.writeImageToStream(waveformImage, memStream);
    imageBuffer.reset();
    imageBuffer.replaceAll(memStream.getData(), memStream.getDataSize());
}
