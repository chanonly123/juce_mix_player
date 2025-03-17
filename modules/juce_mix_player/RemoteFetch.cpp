#include "RemoteFetch.h"

void RemoteFetch::downloadAudioFileWithProgress(const juce::String& urlString,
                                                const juce::File& outputFile,
                                                std::function<void(float progressKb, float totalKb)> progressCallback) {
    juce::URL url(urlString);
    juce::StringPairArray *responseHeaders = new juce::StringPairArray();
    std::unique_ptr<juce::InputStream> inputStream(url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(-1) // infinite
        .withNumRedirectsToFollow(5)
        .withResponseHeaders(responseHeaders)));

    if (!inputStream) {
        throw std::runtime_error("Failed to download file from URL: " + urlString.toStdString());
    }

    juce::int64 totalSize = inputStream->getTotalLength();
    if (totalSize <= 0) {
        throw std::runtime_error("Unknown file size URL: " + urlString.toStdString());
    }

    if (outputFile.exists()) {
//        std::unique_ptr<juce::FileInputStream> inpStream(outputFile.createInputStream());
//        juce::int64 size = ->getTotalLength();
        return;
    }

    outputFile.deleteFile();

    juce::FileOutputStream outputStream(outputFile);
    if (!outputStream.openedOk()) {
        throw std::runtime_error("Failed to open output file: " + outputFile.getFullPathName().toStdString());
    }

    const int bufferSize = 8192; // Read in 8KB chunks
    juce::HeapBlock<char> buffer(bufferSize);
    juce::int64 downloadedSize = 0;

    while (!inputStream->isExhausted()) {
        int bytesRead = inputStream->read(buffer.getData(),
                                          bufferSize);
        if (bytesRead > 0) {
            outputStream
                .write(buffer.getData(),
                       bytesRead);
            downloadedSize += bytesRead;
            
            // Calculate progress
            if (totalSize > 0) {
                progressCallback(static_cast<float>(downloadedSize) / 1024, static_cast<float>(totalSize) / 1024);
            }
        } else {
            break;
        }
    }
    
    PRINT("Download Complete: " + outputFile.getFullPathName());
}

juce::File RemoteFetch::createAbsolutePath(std::string url) {
    juce::String cacheDir;
    if (outputDir == "") {
        cacheDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName() + "/cache/";
    } else {
        cacheDir = outputDir;
    }
    if (!juce::File(cacheDir).exists()) {
        juce::File(cacheDir).createDirectory();
    }
    juce::File file(juce::File::addTrailingSeparator(cacheDir) + hashFilename(url));
    return file;
}

void RemoteFetch::startFetching(std::string url) {
    downloadingUrlOuput[url] = "";
    std::thread([=]() {
        downloadAudioFileWithProgress(url,
                                      createAbsolutePath(url),
                                      [=](float progressKb, float totalKb){
            PRINT("Download Progress " << totalKb << ", " << progressKb);
        });
    }).detach();
}

std::string RemoteFetch::hashFilename(const std::string url) {
    std::hash<std::string> hasher;
    std::size_t hashValue = hasher(url);
    return std::to_string(hashValue);
}

void RemoteFetch::setCacheDirectory(std::string dir) {
    outputDir = dir;
}
