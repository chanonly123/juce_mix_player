#include "GstPlayer.h"
#include "Logger.h"

#if JUCE_IOS
#include "gst_ios_init.h"
#include <mutex>

static std::once_flag gGstInitOnce;

static void ensureGStreamerInitialized() {
    std::call_once(gGstInitOnce, [] {
        PRINT("GstPlayer: calling gst_ios_init()")
        gst_ios_init();
    });
}
#endif

GstPlayer::GstPlayer() {
#if JUCE_IOS
    ensureGStreamerInitialized();
#endif
    PRINT("GstPlayer: created")
}

GstPlayer::~GstPlayer() {
    dispose();
}

void GstPlayer::dispose() {
    stopTimer();
    playing = false;
#if JUCE_IOS
    teardownPipeline();
#endif
}

void GstPlayer::notifyState(JuceMixPlayerState state) {
    if (onStateUpdateCallback) {
        auto s = JuceMixPlayerState_toString(state);
        onStateUpdateCallback(this, returnCopyCharDelete(s.c_str()));
    }
}

void GstPlayer::notifyError(const char* message) {
    if (onErrorCallback) {
        onErrorCallback(this, returnCopyCharDelete(message));
    }
}

void GstPlayer::setVideoPath(const char* path) {
    if (path == nullptr || std::strlen(path) == 0) {
        notifyError("Invalid video path");
        return;
    }
    videoPath = path;
    ready = true;
    progress = 0.0f;
    completed = false;
#if JUCE_IOS
    buildPipelineIfNeeded();
    // set URI
    GError* err = nullptr;
    gchar* uri = gst_filename_to_uri(videoPath.c_str(), &err);
    if (err) {
        notifyError(err->message);
        g_error_free(err);
    } else if (pipeline) {
        g_object_set(pipeline, "uri", uri, nullptr);
        g_free(uri);
    }
    applyOverlayIfAvailable();
#endif
    notifyState(JuceMixPlayerState::READY);
}

void GstPlayer::play() {
    if (!ready) {
        notifyError("Video not set");
        return;
    }
    if (completed) {
        progress = 0.0f;
        completed = false;
#if JUCE_IOS
        if (pipeline) {
            gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
                                    GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                    0);
        }
#endif
    }
    playing = true;
#if JUCE_IOS
    if (pipeline) gst_element_set_state(pipeline, GST_STATE_PLAYING);
#endif
    lastTickMs = juce::Time::getMillisecondCounter();
    startTimer(int(progressUpdateIntervalSec * 1000.0));
    notifyState(JuceMixPlayerState::PLAYING);
}

void GstPlayer::pause() {
    if (!playing) return;
    playing = false;
#if JUCE_IOS
    if (pipeline) gst_element_set_state(pipeline, GST_STATE_PAUSED);
#endif
    stopTimer();
    notifyState(JuceMixPlayerState::PAUSED);
}

void GstPlayer::stop() {
    playing = false;
    stopTimer();
    progress = 0.0f;
    completed = false;
#if JUCE_IOS
    if (pipeline) gst_element_set_state(pipeline, GST_STATE_READY);
#endif
    notifyState(JuceMixPlayerState::STOPPED);
}

void GstPlayer::seek(float normalized) {
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
#if JUCE_IOS
    if (pipeline && durationNs > 0) {
        gint64 target = (gint64)(normalized * (double) durationNs);
        gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
                                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                target);
    }
#endif
    progress = normalized;
    lastTickMs = juce::Time::getMillisecondCounter();
    if (onProgressCallback) onProgressCallback(this, progress);
}

int GstPlayer::isPlaying() {
    return playing ? 1 : 0;
}

float GstPlayer::getDuration() {
#if JUCE_IOS
    if (durationNs > 0) return (float)((double)durationNs / 1e9);
#endif
    return 0.0f;
}

void GstPlayer::setProgressUpdateInterval(float seconds) {
    if (seconds <= 0) return;
    progressUpdateIntervalSec = seconds;
    if (playing) startTimer(int(progressUpdateIntervalSec * 1000.0));
}

void GstPlayer::setMuteEmbeddedAudio(int mute) {
#if JUCE_IOS
    muteEmbedded = (mute != 0);
    if (pipeline) {
        g_object_set(pipeline, "mute", muteEmbedded ? TRUE : FALSE, nullptr);
        g_object_set(pipeline, "volume", muteEmbedded ? 0.0 : 1.0, nullptr);
    }
#endif
}

void GstPlayer::setSurfaceHandle(void* handle) {
    surfaceHandle = handle;
#if JUCE_IOS
    applyOverlayIfAvailable();
#endif
}

void GstPlayer::timerCallback() {
#if JUCE_IOS
    pollBus();
    updateProgressFromPipeline();
#endif
    if (!playing) return;
    // If duration unknown or pipeline not active, simulate minimal progress updates
    auto nowMs = juce::Time::getMillisecondCounter();
    auto deltaMs = nowMs - lastTickMs;
    lastTickMs = nowMs;

#if JUCE_IOS
    if (durationNs <= 0) {
#else
    if (true) { // On non-iOS platforms, always simulate progress
#endif
        float deltaSec = float(deltaMs) / 1000.0f;
        float deltaNorm = deltaSec / kDefaultDurationSec;
        progress = juce::jmin(1.0f, progress + deltaNorm);
        if (onProgressCallback) onProgressCallback(this, progress);
        if (progress >= 1.0f) {
            playing = false;
            stopTimer();
            completed = true;
            notifyState(JuceMixPlayerState::COMPLETED);
        }
    }
}

#if JUCE_IOS
void GstPlayer::buildPipelineIfNeeded() {
    if (pipeline) return;
    pipeline = gst_element_factory_make("playbin", "playbin");
    if (!pipeline) {
        notifyError("Failed to create playbin");
        return;
    }

    // Use GL image sink which supports GstVideoOverlay on iOS
    videoSink = gst_element_factory_make("glimagesink", "videosink");
    if (!videoSink) {
        notifyError("Failed to create glimagesink");
        return;
    }
    g_object_set(videoSink, "force-aspect-ratio", TRUE, nullptr);

    g_object_set(pipeline, "video-sink", videoSink, nullptr);
    g_object_set(pipeline, "mute", muteEmbedded ? TRUE : FALSE, nullptr);
    g_object_set(pipeline, "volume", muteEmbedded ? 0.0 : 1.0, nullptr);

    bus = gst_element_get_bus(pipeline);
}

void GstPlayer::teardownPipeline() {
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
    }
    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }
    if (videoSink) {
        gst_object_unref(videoSink);
        videoSink = nullptr;
    }
    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
    durationNs = 0;
}

void GstPlayer::applyOverlayIfAvailable() {
    if (videoSink && surfaceHandle) {
        if (GST_IS_VIDEO_OVERLAY(videoSink)) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink), (guintptr) surfaceHandle);
        }
    }
}

void GstPlayer::pollBus() {
    if (!bus) return;
    while (true) {
        GstMessage* msg = gst_bus_pop(bus);
        if (!msg) break;
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError* err = nullptr; gchar* dbg = nullptr;
                gst_message_parse_error(msg, &err, &dbg);
                if (err) { notifyError(err->message); g_error_free(err);}
                if (dbg) g_free(dbg);
                playing = false;
                stopTimer();
                notifyState(JuceMixPlayerState::STOPPED);
            } break;
            case GST_MESSAGE_EOS: {
                playing = false;
                stopTimer();
                progress = 1.0f;
                if (onProgressCallback) onProgressCallback(this, progress);
                notifyState(JuceMixPlayerState::COMPLETED);
            } break;
            default: break;
        }
        gst_message_unref(msg);
    }
}

void GstPlayer::updateProgressFromPipeline() {
    if (!pipeline || !playing) return;
    gint64 pos = 0;
    if (!gst_element_query_position(pipeline, GST_FORMAT_TIME, &pos)) return;
    gint64 dur = 0;
    if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur)) {
        durationNs = dur;
    }
    if (durationNs > 0) {
        double norm = (double)pos / (double)durationNs;
        progress = (float) juce::jlimit(0.0, 1.0, norm);
        if (onProgressCallback) onProgressCallback(this, progress);
    }
}
#endif
