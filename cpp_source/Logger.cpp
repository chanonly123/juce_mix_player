#pragma once

#include <JuceHeader.h>

juce::String globalMarker = "";
bool enableLogsValue = true;

// public method to enable/disable logging
void enableLogs() {
    enableLogsValue = true;
}

// enabled logging to release builds as well
#define PRINT(textToWrite) \
    if (enableLogsValue) { \
        juce::String tempDbgBuf; \
        tempDbgBuf << textToWrite; \
        juce::Logger::writeToLog(tempDbgBuf); \
    }
