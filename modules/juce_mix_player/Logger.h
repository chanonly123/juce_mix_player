#pragma once

#include <JuceHeader.h>

extern bool enableLogsValue;

const char* returnCopyCharDelete(std::string str);

const char* returnCopyCharDelete(const char* string);

bool setContains(std::unordered_set<int>& set, int val);

std::string toLower(std::string str);

// enabled logging to release builds as well
#define PRINT(textToWrite) \
    if (enableLogsValue) { \
        juce::String tempDbgBuf; \
        tempDbgBuf << "JUCE: " << textToWrite; \
        juce::Logger::writeToLog(tempDbgBuf); \
    }
