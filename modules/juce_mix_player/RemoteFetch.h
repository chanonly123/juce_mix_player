#pragma once

#include "Logger.h"

class RemoteFetch {
private:

    std::string outputDir;
    std::unique_ptr<juce::InputStream> stream;
    std::unordered_map<std::string, std::string> downloadingUrlOuput;

    juce::File createAbsolutePath(std::string url);
    std::string hashFilename(const std::string url);
    void downloadAudioFileWithProgress(const juce::String& urlString,
                                       const juce::File& outputFile,
                                       std::function<void(float progressKb, float totalKb)> progressCallback);
public:
    RemoteFetch() = default;
    ~RemoteFetch() = default;
    void startFetching(std::string url);
    void setCacheDirectory(std::string dir);
};
