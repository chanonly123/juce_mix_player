#include "JuceMixItem.h"

const char* JuceMixPlayerStateStringValue(JuceMixPlayerState state)
{
    switch (state) {
        case IDLE: return "IDLE";
        case PLAYING: return "PLAYING";
        case PAUSED: return "PAUSED";
        case STOPPED: return "STOPPED";
        case READY: return "READY";
        case LOADING: return "LOADING";
        case ERROR: return "ERROR";
    }
}

JuceMixItem::JuceMixItem() {
    PRINT("JuceMixItem()");
    formatManager.registerBasicFormats();
}

JuceMixItem::~JuceMixItem() {
    PRINT("~JuceMixItem");
    delete reader;
}

// begin time, end time
void JuceMixItem::setPath(juce::String path, float begin, float end) {
    getTaskQueueShared()->async([=]{
        file = juce::File(path);
        readerBuffer = juce::AudioBuffer<float>();
        isPreparing = false;
    });
}

void JuceMixItem::prepare() {
    if (isLoaded()) {
        return;
    }
    if (isPreparing) {
        return;
    }
    isPreparing = true;
    getTaskQueueShared()->async([=]{
        if (file.existsAsFile()) {
            reader = formatManager.createReaderFor(file);
            if (reader != nullptr) {
                readerBuffer = juce::AudioBuffer<float>((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&readerBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
                PRINT("JuceMixItem load: " << file.getFileName() << ", samples: " << reader->lengthInSamples << ", rate: " << reader->sampleRate);
            } else {
                PRINT("JuceMixItem load: " << file.getFileName() << ", unable to load the file");
            }
        } else {
            PRINT("JuceMixItem load: " << file.getFileName() << ", file not exists");
        }
        isPreparing = false;
    });
}

bool JuceMixItem::isLoaded() {
    return readerBuffer.getNumSamples() > 0;
}

float JuceMixItem::getNumSamples() {
    return readerBuffer.getNumSamples();
}

float JuceMixItem::getSampleRate() {
    return reader != nullptr ? reader->sampleRate : 0;
}

juce::AudioBuffer<float>* JuceMixItem::getReadBuffer() {
    return &readerBuffer;
}

std::string JuceMixItem::toString() {
    return "JuceMixItem:" + std::to_string(reinterpret_cast<long>(this));
}

