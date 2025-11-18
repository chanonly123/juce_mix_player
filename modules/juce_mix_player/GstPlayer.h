
#pragma once

#include <JuceHeader.h>
#include "Logger.h"
#include "Models.h"
#include <mutex>

extern "C" {
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
}


// Phase 1 placeholder video player, upgraded to real pipeline on iOS.
class GstPlayer : private juce::Timer {
private:
    std::string videoPath;
    bool ready = false;
    bool playing = false;
    bool completed = false;

    // Native rendering surface handle (platform-specific)
    void* surfaceHandle = nullptr;

    // Normalized progress [0..1]
    float progress = 0.0f;

    // Default assumed duration when real duration unknown
    const float kDefaultDurationSec = 60.0f;

    // Timer / progress update
    double progressUpdateIntervalSec = 0.05; // 50 ms default
    juce::int64 lastTickMs = 0;

#if JUCE_IOS
    GstElement* pipeline = nullptr;
    GstElement* videoSink = nullptr;
    GstBus* bus = nullptr;
    gint64 durationNs = 0;
    bool muteEmbedded = true;

    void buildPipelineIfNeeded();
    void teardownPipeline();
    void applyOverlayIfAvailable();
    void pollBus();
    void updateProgressFromPipeline();
#endif

    void notifyState(JuceMixPlayerState state);
    void notifyError(const char* message);

public:
    // Callbacks (set from C wrappers)
    JuceMixPlayerCallbackFloat onProgressCallback = nullptr;
    JuceMixPlayerCallbackString onStateUpdateCallback = nullptr;
    JuceMixPlayerCallbackString onErrorCallback = nullptr;

    GstPlayer();
    ~GstPlayer();

    void dispose();

    void setVideoPath(const char* path);

    void play();
    void pause();
    void stop();

    // value range 0..1
    void seek(float normalized);

    int isPlaying();

    // In seconds if known, 0 if unknown in this phase
    float getDuration();

    void setProgressUpdateInterval(float seconds);

    // Placeholders for future phases
    void setMuteEmbeddedAudio(int mute);
    void setSurfaceHandle(void* handle);

    // juce::Timer
    void timerCallback() override;
};
