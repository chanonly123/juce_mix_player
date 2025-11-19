#include "GstPlayer.h"
#include "Logger.h"

GstPlayer::GstPlayer() : platform(GstPlatform::create()) {
    platform->initialize();
}

GstPlayer::~GstPlayer() {
    dispose();
}

void GstPlayer::dispose() {
    _stopProgressTimer();
    _isPlaying = false;
    _isPlayingInternal = false;
    teardownPipeline();
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

    // Stop any current playback and tear down existing pipeline so we start clean
    _isPlaying = false;
    _isPlayingInternal = false;
    _isSeeking = false;
    _stopProgressTimer();
    teardownPipeline();

    videoPath = path;
    progress = 0.0f;
    completed = false;
    durationNs = 0;
    ready = false;

    buildPipelineIfNeeded();
    if (!pipeline) {
        notifyError("Failed to initialize video pipeline");
        return;
    }

    // set URI
    GError* err = nullptr;
    gchar* uri = gst_filename_to_uri(videoPath.c_str(), &err);
    if (err != nullptr) {
        notifyError(err->message);
        g_error_free(err);
        teardownPipeline();
        return;
    }

    g_object_set(pipeline, "uri", uri, nullptr);
    g_free(uri);

    applyOverlayIfAvailable();

    ready = true;
    notifyState(JuceMixPlayerState::READY);
}

void GstPlayer::play() {
    if (!ready || !pipeline) {
        notifyError("Video not ready");
        return;
    }
    _playInternal();
}

void GstPlayer::_playInternal() {
    if (!_isPlaying) {
        // If playback had completed previously, restart from the beginning
        if (completed) {
            completed = false;
            progress = 0.0f;
            gst_element_seek_simple(pipeline,
                                    GST_FORMAT_TIME,
                                    GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                    0);
        }

        _isPlaying = true;
        _isPlayingInternal = true;
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        lastTickMs = juce::Time::getMillisecondCounter();
        _startProgressTimer();
        notifyState(JuceMixPlayerState::PLAYING);
    }
}

void GstPlayer::pause() {
    _pauseInternal(false);
}

void GstPlayer::stop() {
    _pauseInternal(true);
}

void GstPlayer::_pauseInternal(bool stop) {
    if (_isPlaying) {
        _isPlaying = false;
        _isPlayingInternal = false;
        if (pipeline) {
            gst_element_set_state(pipeline, stop ? GST_STATE_READY : GST_STATE_PAUSED);
        }
        _stopProgressTimer();
    }
    if (stop) {
        progress = 0.0f;
        completed = false;
        notifyState(JuceMixPlayerState::STOPPED);
    } else {
        notifyState(JuceMixPlayerState::PAUSED);
    }
}

void GstPlayer::seek(float normalized) {
    std::cout << "seek: " << normalized << std::endl;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    if (!pipeline) {
        return;
    }

    _isSeeking = true;

    // Ensure duration is known so we can compute an absolute target
    if (durationNs <= 0) {
        gint64 dur = 0;
        if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur) && dur > 0) {
            durationNs = dur;
        }
    }

    if (durationNs > 0) {
        gint64 target = (gint64)(normalized * (double)durationNs);
        gst_element_seek_simple(pipeline,
                                GST_FORMAT_TIME,
                                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                target);
    }

    progress = normalized;
    lastTickMs = juce::Time::getMillisecondCounter();
    std::cout << "onProgressCallback: " << progress << std::endl;
    if (onProgressCallback) onProgressCallback(this, progress);

    // Clear seeking flag after a brief delay to allow pipeline to settle
    juce::Timer::callAfterDelay(100, [this]() {
        _isSeeking = false;
    });
}

int GstPlayer::isPlaying() {
    return _isPlaying ? 1 : 0;
}

float GstPlayer::getDuration() {
    if (durationNs > 0) {
        return (float)((double)durationNs / 1e9);
    }
    return 0.0f;
}

void GstPlayer::setProgressUpdateInterval(float seconds) {
    if (seconds <= 0) return;
    progressUpdateIntervalSec = seconds;
    if (_isPlaying) _startProgressTimer();
}

void GstPlayer::_startProgressTimer() {
    if (!isTimerRunning()) {
        startTimer(int(progressUpdateIntervalSec * 1000.0));
    }
}

void GstPlayer::_stopProgressTimer() {
    if (isTimerRunning() && !_isPlaying) {
        stopTimer();
    }
}

void GstPlayer::setMuteEmbeddedAudio(int mute) {
    muteEmbedded = (mute != 0);
    if (pipeline) {
        g_object_set(pipeline, "mute", muteEmbedded ? TRUE : FALSE, nullptr);
        g_object_set(pipeline, "volume", muteEmbedded ? 0.0 : 1.0, nullptr);
    }
}

void GstPlayer::setSurfaceHandle(void* handle) {
    surfaceHandle = handle;
    applyOverlayIfAvailable();

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
    if (videoFlip) {
        g_object_set(videoFlip, "method", static_cast<int>(newRotation), nullptr);
    }
    std::cout << "ROTATION APPLIED: " << degrees << std::endl;
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

    safelyReplaceEffectFilter();
    std::cout << "VIDEO FILTER APPLIED: " << effectId << std::endl;
}

void GstPlayer::exportVideo(const char* outputPath, void (*completion)(const char*)) {
    if (!ready || !pipeline) {
        completion("Video not ready for export");
        return;
    }

    if (_isPlayingInternal) {
        completion("Cannot export while playing. Please pause first.");
        return;
    }

    // TODO: Implement actual GStreamer encoding pipeline
    completion("");
}

void GstPlayer::timerCallback() {
    pollBus();

    // Only update progress when not seeking and actually playing
    if (!_isSeeking && _isPlayingInternal && _isPlaying) {
        updateProgressFromPipeline();
    }
}

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
    // First, set pipeline to NULL state and wait for state change
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        // Wait for state change to complete
        GstState state;
        gst_element_get_state(pipeline, &state, nullptr, GST_CLOCK_TIME_NONE);
    }

    // Now safely unref all elements
    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }

    // Note: videoBin owns videoSink, videoFlip, and effectFilter, so we don't unref them individually
    if (videoBin) {
        gst_object_unref(videoBin);
        videoBin = nullptr;
        // Clear references to child elements
        videoSink = nullptr;
        videoFlip = nullptr;
        effectFilter = nullptr;
    }

    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    durationNs = 0;

    // if (platform) {
    //     platform->cleanup();
    // }
}

void GstPlayer::applyOverlayIfAvailable() {
    if (!videoSink || !surfaceHandle) return;
    if (GST_IS_VIDEO_OVERLAY(videoSink)) {
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink), (guintptr) surfaceHandle);
    }
}

void GstPlayer::setupVideoProcessingBin() {
    videoBin = gst_bin_new("video-processing-bin");
    if (!videoBin) {
        notifyError("Failed to create video processing bin");
        return;
    }
    videoFlip = gst_element_factory_make("videoflip", "videoflip");
    if (!videoFlip) {
        notifyError("Failed to create videoflip element");
        return;
    }
    effectFilter = makeEffectFilter(currentEffect);
    if (!effectFilter) {
        notifyError("Failed to create effect filter");
        return;
    }
    videoSink = makeVideoSink();
    if (!videoSink) {
        notifyError("Failed to create video sink");
        return;
    }
    GstElement* converter = gst_element_factory_make("videoconvert", "effect-converter");
    if (!converter) {
        notifyError("Failed to create videoconvert element");
        return;
    }
    gst_bin_add_many(GST_BIN(videoBin), videoFlip, converter, effectFilter, videoSink, nullptr);
    if (!gst_element_link_many(videoFlip, converter, effectFilter, videoSink, nullptr)) {
        notifyError("Failed to link video processing elements");
        return;
    }
    GstPad* sinkPad = gst_element_get_static_pad(videoFlip, "sink");
    gst_element_add_pad(videoBin, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);

    applyEffectParams(effectFilter, currentEffect);


    // g_object_set(videoFlip, "method", static_cast<int>(currentRotation), nullptr);
    
}

void GstPlayer::safelyReplaceEffectFilter() {
    if (!pipeline || !videoBin) return;
    // Simple approach: just replace the effect filter, keep the same pipeline structure
    if (effectFilter) {
        // Set to NULL state first
        gst_element_set_state(effectFilter, GST_STATE_NULL);

        // Get the converter element (should be linked before effectFilter)
        GstElement* converter = gst_bin_get_by_name(GST_BIN(videoBin), "effect-converter");

        // Unlink and remove old effect filter
        if (converter) {
            gst_element_unlink(converter, effectFilter);
        }
        gst_element_unlink(effectFilter, videoSink);
        gst_bin_remove(GST_BIN(videoBin), effectFilter);
        effectFilter = nullptr;

        effectFilter = makeEffectFilter(currentEffect);
        if (!effectFilter) {
            notifyError("Failed to create new effect filter");
            return;
        }

        // Add to bin
        gst_bin_add(GST_BIN(videoBin), effectFilter);

        applyEffectParams(effectFilter, currentEffect);

        // Relink: converter -> effectFilter -> videoSink
        if (converter) {
            if (!gst_element_link(converter, effectFilter)) {
                notifyError("Failed to link converter to new effect filter");
                return;
            }
        }
        if (!gst_element_link(effectFilter, videoSink)) {
            notifyError("Failed to link new effect filter to video sink");
            return;
        }

        // Sync state
        gst_element_sync_state_with_parent(effectFilter);

        if (converter) {
            gst_object_unref(converter);
        }
    }
}

GstElement* GstPlayer::makeVideoSink() {
    GstElement* sink = gst_element_factory_make("glimagesink", "videosink");
    if (sink) {
        g_object_set(sink, "force-aspect-ratio", TRUE, nullptr);
    }
    return sink;
}

GstElement* GstPlayer::makeEffectFilter(VisualEffect effect) {
    const char* filterName = "identity";
    switch (effect) {
        case VisualEffect::NONE:
            filterName = "identity";
            break;
        case VisualEffect::GRAINY:
        case VisualEffect::GRITTY:
        case VisualEffect::HYPER:
            filterName = "videobalance";
            break;
    }
    return gst_element_factory_make(filterName, "effectfilter");
}

void GstPlayer::applyEffectParams(GstElement* effectFilter, VisualEffect effect) {
    if (!effectFilter) return;
    if (effect == VisualEffect::NONE) return;
    GstElementFactory* factory = gst_element_get_factory(effectFilter);
    if (!factory) return;
    const gchar* factoryName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    if (g_strcmp0(factoryName, "videobalance") != 0) return;
    std::map<std::string, std::string> params;
    switch (effect) {
        case VisualEffect::GRAINY:
            params["brightness"] = "-0.05";
            params["contrast"]   = "0.75";
            params["saturation"] = "0.9";
            params["hue"]        = "0.0";
            break;
        case VisualEffect::GRITTY:
            params["brightness"] = "-0.10";
            params["contrast"]   = "1.35";
            params["saturation"] = "0.55";
            params["hue"]        = "0.0";
            break;
        case VisualEffect::HYPER:
            params["brightness"] = "0.10";
            params["contrast"]   = "1.5";
            params["saturation"] = "1.8";
            params["hue"]        = "0.06";
            break;
        default:
            break;
    }
    for (const auto& p : params) {
        g_object_set(effectFilter, p.first.c_str(), std::stod(p.second), nullptr);
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
                _isPlaying = false;
                _isPlayingInternal = false;
                _stopProgressTimer();
                notifyState(JuceMixPlayerState::STOPPED);
            } break;
            case GST_MESSAGE_EOS: {
                _isPlaying = false;
                _isPlayingInternal = false;
                completed = true;
                _stopProgressTimer();
                progress = 1.0f;
                if (onProgressCallback) onProgressCallback(this, progress);
                std::cout << "pollBus: onProgressCallback: " << progress << std::endl;
                notifyState(JuceMixPlayerState::COMPLETED);
            } break;
            default: break;
        }
        gst_message_unref(msg);
    }
}

void GstPlayer::updateProgressFromPipeline() {
    if (!pipeline) return;

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
        std::cout << "updateProgressFromPipeline: " << progress << std::endl;
    }
}
