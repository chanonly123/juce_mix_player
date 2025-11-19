
#pragma once

#include <JuceHeader.h>
#include "Logger.h"
#include "Models.h"
#include "GstPlatform.h"
#include <mutex>
#include <map>
#include <string>
#include <memory>

extern "C" {
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
}

// Video rotation enum
enum class VideoRotation {
    ROTATE_0 = 0,
    ROTATE_90 = 1,
    ROTATE_180 = 2,
    ROTATE_270 = 3
};

// Visual effects enum
enum class VisualEffect {
    NONE = 0,
    GRAINY = 1,
    GRITTY = 2,
    HYPER = 3
};


// Phase 1 placeholder video player, upgraded to real pipeline on iOS.
class GstPlayer : private juce::Timer {
private:
    std::string videoPath;
    bool ready = false;
    bool _isPlaying = false;        // User intent to play
    bool _isPlayingInternal = false; // Actual pipeline state
    bool _isSeeking = false;        // Prevent progress updates during seek
    bool completed = false;

    // Native rendering surface handle (platform-specific)
    void* surfaceHandle = nullptr;

    // Normalized progress [0..1]
    float progress = 0.0f;

    // Timer / progress update
    double progressUpdateIntervalSec = 0.05; // 50 ms default
    juce::int64 lastTickMs = 0;
    juce::int64 lastSeekMs = 0; // Track when we last seeked to avoid position reset artifacts

    // Video processing state
    VisualEffect currentEffect = VisualEffect::NONE;
    // Platform abstraction
    std::unique_ptr<GstPlatform> platform;

    // GStreamer elements (now cross-platform)
    GstElement* pipeline = nullptr;
    GstElement* videoSink = nullptr;
    GstElement* videoFlip = nullptr;
    GstElement* effectFilter = nullptr;
    GstElement* videoBin = nullptr;
    GstBus* bus = nullptr;
    gint64 durationNs = 0;
    bool muteEmbedded = true;

    // Pipeline management methods
    void buildPipelineIfNeeded();
    void teardownPipeline();
    void safelyReplaceEffectFilter();
    void applyOverlayIfAvailable();
    void pollBus();
    void updateProgressFromPipeline();
    void setupVideoProcessingBin();
    GstElement* makeVideoSink();
    GstElement* makeEffectFilter(VisualEffect effect);
    void applyEffectParams(GstElement* effectFilter, VisualEffect effect);

    // Internal state management (following JuceMixPlayer pattern)
    void _playInternal();
    void _pauseInternal(bool stop);
    void _startProgressTimer();
    void _stopProgressTimer();

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

    // Video processing methods
    void setRotation(int degrees);
    void setVisualEffect(int effectId);
    void exportVideo(const char* outputPath, void (*completion)(const char*));

    // Placeholders for future phases
    void setMuteEmbeddedAudio(int mute);
    void setSurfaceHandle(void* handle);

    // juce::Timer
    void timerCallback() override;
};
