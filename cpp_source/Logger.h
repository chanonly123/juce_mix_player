#pragma once

#include <JuceHeader.h>

extern bool enableLogsValue;

// enabled logging to release builds as well
#define PRINT(textToWrite) \
    if (enableLogsValue) { \
        juce::String tempDbgBuf; \
        tempDbgBuf << textToWrite; \
        juce::Logger::writeToLog(tempDbgBuf); \
    }
