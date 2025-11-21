// GstPlayer.h
#pragma once

#include <JuceHeader.h>
#include "Logger.h"
#include "Models.h"
#include "GstPlatform.h"
#include "TaskQueue.h"
#include <mutex>
#include <map>
#include <string>
#include <memory>
#include <atomic>

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

class GstPlayer : private juce::Timer {
private:
    std::string videoPath;
    bool ready = false;
    bool _isPlaying = false;
    bool _isPlayingInternal = false;
    bool _isSeeking = false;
    bool completed = false;

    void* surfaceHandle = nullptr;

    // NEW: atomic copy for bus sync thread
    std::atomic<guintptr> surfaceHandleAtomic { 0 };

    float progress = 0.0f;

    double progressUpdateIntervalSec = 0.10;

    VisualEffect currentEffect = VisualEffect::NONE;
    std::unique_ptr<GstPlatform> platform;

    GstElement* pipeline = nullptr;
    GstElement* videoSink = nullptr;
    GstElement* videoFlip = nullptr;
    GstElement* effectFilter = nullptr;
    GstElement* videoBin = nullptr;
    GstBus* bus = nullptr;
    gint64 durationMs = 0;
    bool muteEmbedded = true;
    double lastSeekMs = 0;

    TaskQueue gstTaskQueue;

    void buildPipelineIfNeeded();
    void teardownPipeline();
    void safelyReplaceEffectFilter();
    void applyOverlayIfAvailable();
    void pollBus();
    void setupVideoProcessingBin();
    GstElement* makeVideoSink();
    GstElement* makeEffectFilter(VisualEffect effect);
    void applyEffectParams(GstElement* effectFilter, VisualEffect effect);

    void _playInternal();
    void _pauseInternal(bool stop);
    void _startProgressTimer();
    void _stopProgressTimer();
    gint64 _getDurationInternal();

    void notifyState(JuceMixPlayerState state);
    void notifyError(const char* message);

    // NEW: bus sync handler (static member can access private)
    static GstBusSyncReply bus_sync_cb(GstBus* bus, GstMessage* msg, gpointer user_data);

public:
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
    void seek(float normalized);
    int isPlaying();
    float getDurationInSecs();
    void setProgressUpdateInterval(float seconds);
    void setRotation(int degrees);
    void setVisualEffect(int effectId);
    void exportVideo(const char* outputPath, std::function<void(const char*)> completion);
    void setMuteEmbeddedAudio(int mute);
    void setSurfaceHandle(void* handle);

    void timerCallback() override;
};

