// GstPlayer.h
#pragma once

#include "GstPlatform.h"
#include "Logger.h"
#include "Models.h"
#include "TaskQueue.h"
#include <JuceHeader.h>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
}

// Visual effects enum
enum class VisualEffect {
    NONE = 0,
    GRAINY = 1,
    GRITTY = 2,
    HYPER = 3,
};

class GstPlayer : private juce::Timer {
private:
    std::unique_ptr<GstPlatform> platform;
    void *surfaceHandle = nullptr;
    std::atomic<guintptr> surfaceHandleAtomic{0};
    TaskQueue gstTaskQueue;
    
    std::string _videoPath;
    bool _isReady = false;
    bool _isPlaying = false;
    bool _isPlayingInternal = false;
    bool _isSeeking = false;
    bool _isCompleted = false;
    float _progress = 0.0f;
    int _currentRotation = 0;
    // Flip state (0-8)
    int _currentFlipMethod = 0;
    gint64 _durationMs = 0;
    bool _muteEmbedded = true;
    double _lastSeekMs = 0;
    VisualEffect _currentEffect = VisualEffect::NONE;
    bool _blackOverlayEnabled = false;
    
    int _trimStartMs = 0.0;
    int _trimEndMs = 0.0;
    
    double progressUpdateIntervalSec = 0.10;
    
    GstElement *pipeline = nullptr;
    GstElement *source = nullptr;
    GstElement *decodebin = nullptr;
    GstElement *videoQueue = nullptr;
    GstElement *videoConvert = nullptr;
    GstElement *videoSink = nullptr;
    GstElement *videoRotate = nullptr;
    GstElement *videoFlip = nullptr;
    GstElement *videoBalance = nullptr;
    GstElement *videoBin = nullptr;
    GstElement *audioConvert = nullptr;
    GstElement *audioVolume = nullptr;
    GstElement *audioSink = nullptr;
    GstBus *bus = nullptr;
    
    // Track dynamically allocated data for cleanup
    std::pair<GstElement *, GstElement *> *decodebinPadData = nullptr;
    
    void buildPipeline();
    void teardownPipeline();
    void safelyReplaceEffectFilter();
    void applyOverlay();
    void pollBus();
    void setupVideoProcessingBin();
    void applyEffectParams(VisualEffect effect);
    
    void _playInternal();
    void _pauseInternal(bool stop);
    void _startProgressTimer();
    void _stopProgressTimer();
    gint64 _getDurationInternal();
    
    void notifyState(JuceMixPlayerState state);
    void notifyError(const char *message);
    
public:
    JuceMixPlayerCallbackFloat onProgressCallback = nullptr;
    JuceMixPlayerCallbackString onStateUpdateCallback = nullptr;
    JuceMixPlayerCallbackString onErrorCallback = nullptr;
    
    // User context for callbacks (e.g., parent wrapper object)
    void *userContext = nullptr;
    
    GstPlayer();
    ~GstPlayer();
    
    void dispose();
    void setVideoPath(const char *path);
    void play();
    void pause();
    void stop();
    void seek(float normalized);
    int isPlaying();
    float getDurationInSecs();
    void setProgressUpdateInterval(float seconds);
    void setRotation(int degrees);
    void setFlip(int method);
    void setVisualEffect(int effectId);
    void setBlackOverlayEnabled(int enabled);
    void setTrimRange(int startMs, int endMs);
    int getTrimStart();
    int getTrimEnd();
    void exportVideo(const char *outputPath, std::function<void(const char *)> completion);
    void setMuteEmbeddedAudio(int mute);
    void setSurfaceHandle(void *handle);
    void timerCallback() override;
};
