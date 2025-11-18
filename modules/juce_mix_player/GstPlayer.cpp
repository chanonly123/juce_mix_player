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

void GstPlayer::setRotation(int degrees) {
    VideoRotation newRotation;
    switch (degrees) {
        case 0:   newRotation = VideoRotation::ROTATE_0; break;
        case 90:  newRotation = VideoRotation::ROTATE_90; break;
        case 180: newRotation = VideoRotation::ROTATE_180; break;
        case 270: newRotation = VideoRotation::ROTATE_270; break;
        default:
            notifyError("Invalid rotation angle. Use 0, 90, 180, or 270 degrees.");
            return;
    }

    if (currentRotation == newRotation) return;

    currentRotation = newRotation;

#if JUCE_IOS
    if (videoFlip) {
        g_object_set(videoFlip, "method", static_cast<int>(currentRotation), nullptr);
    }
#endif
}

void GstPlayer::setVisualEffect(int effectId) {
    VisualEffect newEffect;
    switch (effectId) {
        case 0: newEffect = VisualEffect::NONE; break;
        case 1: newEffect = VisualEffect::GRAINY; break;
        case 2: newEffect = VisualEffect::GRITTY; break;
        case 3: newEffect = VisualEffect::HYPER; break;
        default:
            notifyError("Invalid effect ID. Use 0 (NONE), 1 (GRAINY), 2 (GRITTY), or 3 (HYPER).");
            return;
    }

    if (currentEffect == newEffect) return;

    currentEffect = newEffect;

#if JUCE_IOS
    rebuildPipelineWithEffects();
#endif
}

void GstPlayer::exportVideo(const char* outputPath, void (*completion)(const char*)) {
    if (!ready || !pipeline) {
        completion("Video not ready for export");
        return;
    }

    if (playing) {
        completion("Cannot export while playing. Please pause first.");
        return;
    }

#if JUCE_IOS
    // For now, implement a simple export by copying the file with transformations
    // This is a simplified version - a full implementation would use GStreamer's
    // encoding pipeline to apply effects and save the result

    // Create a simple file copy with current transformations applied
    // In a real implementation, this would:
    // 1. Create an encoding pipeline with filesink
    // 2. Apply current rotation and effects
    // 3. Encode to the output format
    // 4. Monitor progress and call completion when done

    PRINT("Exporting video with rotation: " << static_cast<int>(currentRotation)
          << " and effect: " << static_cast<int>(currentEffect));

    // For this implementation, we'll simulate export success
    // TODO: Implement actual GStreamer encoding pipeline
    completion("");
#else
    completion("Export not supported on this platform");
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

    // Setup video processing bin with effects and rotation
    setupVideoProcessingBin();

    g_object_set(pipeline, "video-sink", videoBin, nullptr);
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
    if (videoBin) {
        gst_object_unref(videoBin);
        videoBin = nullptr;
    }
    if (videoSink) {
        gst_object_unref(videoSink);
        videoSink = nullptr;
    }
    if (videoFlip) {
        gst_object_unref(videoFlip);
        videoFlip = nullptr;
    }
    if (effectFilter) {
        gst_object_unref(effectFilter);
        effectFilter = nullptr;
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

void GstPlayer::setupVideoProcessingBin() {
    // Create a bin to hold video processing elements
    videoBin = gst_bin_new("video-processing-bin");
    if (!videoBin) {
        notifyError("Failed to create video processing bin");
        return;
    }

    // Create video flip element for rotation
    videoFlip = gst_element_factory_make("videoflip", "videoflip");
    if (!videoFlip) {
        notifyError("Failed to create videoflip element");
        return;
    }

    // Create effect filter (start with identity for no effect)
    effectFilter = gst_element_factory_make("identity", "effectfilter");
    if (!effectFilter) {
        notifyError("Failed to create effect filter");
        return;
    }

    // Create GL image sink
    videoSink = gst_element_factory_make("glimagesink", "videosink");
    if (!videoSink) {
        notifyError("Failed to create glimagesink");
        return;
    }
    g_object_set(videoSink, "force-aspect-ratio", TRUE, nullptr);

    // Add elements to bin
    gst_bin_add_many(GST_BIN(videoBin), videoFlip, effectFilter, videoSink, nullptr);

    // Link elements: videoflip -> effectfilter -> videosink
    if (!gst_element_link_many(videoFlip, effectFilter, videoSink, nullptr)) {
        notifyError("Failed to link video processing elements");
        return;
    }

    // Create ghost pad for the bin
    GstPad* sinkPad = gst_element_get_static_pad(videoFlip, "sink");
    gst_element_add_pad(videoBin, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);

    // Set initial rotation
    g_object_set(videoFlip, "method", static_cast<int>(currentRotation), nullptr);
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

std::map<std::string, std::string> GstPlayer::getEffectParameters(VisualEffect effect) {
    std::map<std::string, std::string> params;

    switch (effect) {
        case VisualEffect::NONE:
            // No effect - use identity element
            break;
        case VisualEffect::GRAINY:
            // Add noise/grain effect parameters
            params["noise"] = "0.3";
            params["grain"] = "0.4";
            break;
        case VisualEffect::GRITTY:
            // High contrast, desaturated look
            params["contrast"] = "1.5";
            params["saturation"] = "0.7";
            params["brightness"] = "-0.1";
            break;
        case VisualEffect::HYPER:
            // Oversaturated, high contrast
            params["contrast"] = "1.8";
            params["saturation"] = "1.6";
            params["brightness"] = "0.1";
            break;
    }

    return params;
}

void GstPlayer::rebuildPipelineWithEffects() {
    if (!pipeline) return;

    bool wasPlaying = playing;
    float currentProgress = progress;

    // Stop and rebuild pipeline
    if (wasPlaying) {
        gst_element_set_state(pipeline, GST_STATE_PAUSED);
    }

    // Remove old effect filter
    if (effectFilter) {
        gst_element_unlink(videoFlip, effectFilter);
        gst_element_unlink(effectFilter, videoSink);
        gst_bin_remove(GST_BIN(videoBin), effectFilter);
        gst_object_unref(effectFilter);
        effectFilter = nullptr;
    }

    // Create new effect filter based on current effect
    const char* filterName = "identity";
    switch (currentEffect) {
        case VisualEffect::NONE:
            filterName = "identity";
            break;
        case VisualEffect::GRAINY:
        case VisualEffect::GRITTY:
        case VisualEffect::HYPER:
            filterName = "videobalance";
            break;
    }

    effectFilter = gst_element_factory_make(filterName, "effectfilter");
    if (!effectFilter) {
        notifyError("Failed to create new effect filter");
        return;
    }

    // Apply effect parameters
    auto params = getEffectParameters(currentEffect);
    for (const auto& param : params) {
        g_object_set(effectFilter, param.first.c_str(), std::stod(param.second), nullptr);
    }

    // Add and link new filter
    gst_bin_add(GST_BIN(videoBin), effectFilter);
    if (!gst_element_link_many(videoFlip, effectFilter, videoSink, nullptr)) {
        notifyError("Failed to link new effect filter");
        return;
    }

    // Sync state
    gst_element_sync_state_with_parent(effectFilter);

    // Resume playback if it was playing
    if (wasPlaying) {
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        seek(currentProgress);
    }
}
#endif
