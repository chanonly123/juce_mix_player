#include "GstPlayer.h"
#include "Logger.h"
#include <gst/gst.h>
#include <thread>
#include <cstring>

// Constructor: initialize platform
GstPlayer::GstPlayer() : platform(GstPlatform::create()) {
    platform->initialize();
}

// Destructor: ensure resources are freed
GstPlayer::~GstPlayer() {
    dispose();
}

void GstPlayer::dispose() {
    // Stop any running timers and reset state
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

    // Stop current playback and tear down the existing pipeline to start clean
    _isPlaying = false;
    _isPlayingInternal = false;
    _isSeeking = false;
    _stopProgressTimer();
    teardownPipeline();

    videoPath = path;
    progress    = 0.0f;
    completed   = false;
    durationNs  = 0;
    ready       = false;

    buildPipelineIfNeeded();
    if (!pipeline) {
        notifyError("Failed to initialize video pipeline");
        return;
    }

    // Set media URI for playbin
    GError* err = nullptr;
    gchar* uri  = gst_filename_to_uri(videoPath.c_str(), &err);
    if (err != nullptr) {
        notifyError(err->message);
        g_error_free(err);
        teardownPipeline();
        return;
    }
    g_object_set(pipeline, "uri", uri, nullptr);
    g_free(uri);

    // Apply video overlay (window handle) if available
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
        // If playback had completed previously, restart from beginning
        if (completed) {
            completed = false;
            progress  = 0.0f;
            gst_element_seek_simple(
                pipeline, GST_FORMAT_TIME,
                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                0);
        }

        _isPlaying        = true;
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
    // Always set pipeline to the appropriate state (PAUSED or READY) when pausing/stopping
    if (pipeline) {
        gst_element_set_state(pipeline, stop ? GST_STATE_READY : GST_STATE_PAUSED);
    }
    _stopProgressTimer();

    _isPlaying = false;
    _isPlayingInternal = false;
    if (stop) {
        // Reset progress on stop
        progress  = 0.0f;
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

    // Ensure duration is known (query if not cached) so we can seek to an absolute target
    gint64 dur = 0;
    if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur) && dur > 0) {
        durationNs = dur;
    } else {
        durationNs = 0; // fallback
    }
    if (durationNs > 0) {
        gint64 target = (gint64)((double)durationNs * normalized);
        gst_element_seek_simple(
            pipeline, GST_FORMAT_TIME,
            GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
            target);
    }

    // Immediately update our progress value and notify
    progress   = normalized;
    lastTickMs = juce::Time::getMillisecondCounter();
    std::cout << "onProgressCallback: " << progress << std::endl;
    if (onProgressCallback) {
        onProgressCallback(this, progress);
    }

    // Clear the seeking flag after a brief delay to allow the pipeline to settle
    juce::Timer::callAfterDelay(100, [this]() {
        _isSeeking = false;
    });

}

int GstPlayer::isPlaying() {
    return _isPlaying ? 1 : 0;
}

float GstPlayer::getDuration() {
    if (durationNs > 0) {
        // Convert nanoseconds to seconds
        return (float)((double)durationNs / 1e9);
    }
    return 0.0f;
}

void GstPlayer::setProgressUpdateInterval(float seconds) {
    if (seconds <= 0) return;
    progressUpdateIntervalSec = seconds;
    if (_isPlaying) {
        _startProgressTimer();
    }
}

void GstPlayer::_startProgressTimer() {
    if (!isTimerRunning()) {
        startTimer(int(progressUpdateIntervalSec * 1000.0f));
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
        // Mute or unmute the audio in the playbin
        g_object_set(pipeline, 
                     "mute",   muteEmbedded ? TRUE : FALSE,
                     "volume", muteEmbedded ? 0.0  : 1.0, 
                     nullptr);
    }
}

void GstPlayer::setSurfaceHandle(void* handle) {
    surfaceHandle = handle;
    applyOverlayIfAvailable();
}

void GstPlayer::setRotation(int degrees) {
    VideoRotation newRotation;
    switch (degrees) {
        case 0:   newRotation = VideoRotation::ROTATE_0;   break;
        case 90:  newRotation = VideoRotation::ROTATE_90;  break;
        case 180: newRotation = VideoRotation::ROTATE_180; break;
        case 270: newRotation = VideoRotation::ROTATE_270; break;
        default:
            notifyError("Invalid rotation angle. Use 0, 90, 180, or 270 degrees.");
            return;
    }
    // Apply rotation if the videoFlip element exists
    if (videoFlip) {
        g_object_set(videoFlip, "method", static_cast<int>(newRotation), nullptr);
    }
    std::cout << "ROTATION APPLIED: " << degrees << std::endl;
}

void GstPlayer::setVisualEffect(int effectId) {
    VisualEffect newEffect;
    switch (effectId) {
        case 0: newEffect = VisualEffect::NONE;   break;
        case 1: newEffect = VisualEffect::GRAINY; break;
        case 2: newEffect = VisualEffect::GRITTY; break;
        case 3: newEffect = VisualEffect::HYPER;  break;
        default:
            notifyError("Invalid effect ID. Use 0 (NONE), 1 (GRAINY), 2 (GRITTY), or 3 (HYPER).");
            return;
    }
    if (currentEffect == newEffect) {
        // No change needed
        return;
    }
    currentEffect = newEffect;

    // Apply new effect parameters to the existing filter in real-time
    if (effectFilter) {
        applyEffectParams(effectFilter, currentEffect);
    }
    std::cout << "VIDEO FILTER APPLIED: " << effectId << std::endl;
}

static void onPadAdded(GstElement* src, GstPad* pad, gpointer data) {
    auto elems = static_cast<std::pair<GstElement*, GstElement*>*>(data);
    GstCaps* caps = gst_pad_get_current_caps(pad);
    const gchar* name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    GstPad* sinkPad = nullptr;

    if (g_str_has_prefix(name, "video/")) {
        sinkPad = gst_element_get_static_pad(elems->first, "sink");
    } else if (g_str_has_prefix(name, "audio/")) {
        sinkPad = gst_element_get_static_pad(elems->second, "sink");
    }

    if (sinkPad && !gst_pad_is_linked(sinkPad)) {
        gst_pad_link(pad, sinkPad);
    }

    if (caps) gst_caps_unref(caps);
    if (sinkPad) gst_object_unref(sinkPad);
}


void GstPlayer::exportVideo(const char* outputPath, void (*completion)(int)) {
    if (!ready || videoPath.empty()) {
        notifyError("No video loaded");
        return;
    }
    if (_isPlayingInternal) {
        notifyError("Cannot export while playing. Please pause first.");
        return;
    }

    std::string inputPath = videoPath;
    std::string outputPathStr = outputPath;
    VisualEffect effect = currentEffect;

    int rotationMethod = 0;
    if (videoFlip) {
        g_object_get(videoFlip, "method", &rotationMethod, nullptr);
    }

    std::thread([this, inputPath, outputPathStr, effect, rotationMethod]() {
        GstElement* pipeline = gst_pipeline_new("export_pipeline");
        GstElement* src = gst_element_factory_make("filesrc", "src");
        GstElement* decodebin = gst_element_factory_make("decodebin", "decodebin");
        GstElement* videoQueue = gst_element_factory_make("queue", "video_queue");
        GstElement* audioQueue = gst_element_factory_make("queue", "audio_queue");
        GstElement* balance = gst_element_factory_make("videobalance", "balance");
        GstElement* flip = gst_element_factory_make("videoflip", "flip");
        GstElement* vconv = gst_element_factory_make("videoconvert", "vconv");
        GstElement* x264enc = gst_element_factory_make("x264enc", "x264enc");
        GstElement* h264parse = gst_element_factory_make("h264parse", "h264parse");
        GstElement* aconv = gst_element_factory_make("audioconvert", "aconv");
        GstElement* aresample = gst_element_factory_make("audioresample", "aresample");
        GstElement* aacenc = gst_element_factory_make("voaacenc", "aacenc");
        GstElement* aacparse = gst_element_factory_make("aacparse", "aacparse");
        GstElement* mp4mux = gst_element_factory_make("mp4mux", "mux");
        GstElement* sink = gst_element_factory_make("filesink", "sink");

        if (!pipeline || !src || !decodebin || !videoQueue || !audioQueue || !balance || !flip || !vconv ||
            !x264enc || !h264parse || !aconv || !aresample || !aacenc || !aacparse || !mp4mux || !sink) {
            notifyError("Failed to create GStreamer export pipeline");
            return;
        }

        g_object_set(src, "location", inputPath.c_str(), nullptr);
        g_object_set(sink, "location", outputPathStr.c_str(), "sync", FALSE, nullptr);
        g_object_set(flip, "method", rotationMethod, nullptr);
        applyEffectParams(balance, effect);

        gst_bin_add_many(GST_BIN(pipeline), src, decodebin, videoQueue, balance, flip, vconv,
                         x264enc, h264parse, audioQueue, aconv, aresample, aacenc, aacparse,
                         mp4mux, sink, nullptr);

        gst_element_link(src, decodebin);
        gst_element_link_many(videoQueue, balance, flip, vconv, x264enc, h264parse, nullptr);
        gst_element_link_many(audioQueue, aconv, aresample, aacenc, aacparse, nullptr);
        gst_element_link(mp4mux, sink);
        
        auto* decodeTargets = new std::pair<GstElement*, GstElement*>(videoQueue, audioQueue);
        g_signal_connect(decodebin, "pad-added", G_CALLBACK(onPadAdded), decodeTargets);

        GstPad* videoSinkPad = gst_element_request_pad_simple(mp4mux, "video_0");
        GstPad* audioSinkPad = gst_element_request_pad_simple(mp4mux, "audio_0");
        GstPad* videoSrcPad = gst_element_get_static_pad(h264parse, "src");
        GstPad* audioSrcPad = gst_element_get_static_pad(aacparse, "src");
        gst_pad_link(videoSrcPad, videoSinkPad);
        gst_pad_link(audioSrcPad, audioSinkPad);
        gst_object_unref(videoSinkPad);
        gst_object_unref(audioSinkPad);
        gst_object_unref(videoSrcPad);
        gst_object_unref(audioSrcPad);

        gst_element_set_state(pipeline, GST_STATE_PLAYING);

        GstBus* bus = gst_element_get_bus(pipeline);
        GstMessage* msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                          (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError* err;
                gst_message_parse_error(msg, &err, nullptr);
                notifyError(err->message);
                g_error_free(err);
            } else {
                // Success — optionally notify user or log
                std::cout << "Export completed: " << outputPathStr << std::endl;
            }
            gst_message_unref(msg);
        }

        gst_object_unref(bus);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        delete decodeTargets;
    }).detach();
}



void GstPlayer::timerCallback() {
    // Poll the bus for messages (error, EOS, etc.)
    pollBus();
    // Update progress periodically if playing and not currently seeking
    if (!_isSeeking && _isPlayingInternal && _isPlaying) {
        updateProgressFromPipeline();
    }
}

void GstPlayer::buildPipelineIfNeeded() {
    if (pipeline) return;  // pipeline already built

    pipeline = gst_element_factory_make("playbin", "playbin");
    if (!pipeline) {
        notifyError("Failed to create playbin");
        return;
    }

    // Set up the custom video output bin (with rotation & effect filter)
    setupVideoProcessingBin();
    if (!videoBin) {
        // setupVideoProcessingBin already reported the error
        return;
    }

    // Instruct playbin to use our video bin as the video output sink
    g_object_set(pipeline, "video-sink", videoBin, nullptr);
    // Apply initial audio mute/volume settings
    g_object_set(pipeline, 
                 "mute",   muteEmbedded ? TRUE : FALSE,
                 "volume", muteEmbedded ? 0.0  : 1.0, 
                 nullptr);

    // Get the message bus for this pipeline
    bus = gst_element_get_bus(pipeline);
}

void GstPlayer::teardownPipeline() {
    if (pipeline) {
        // Stop pipeline: set to NULL state (this will also internally stop playback threads)
        gst_element_set_state(pipeline, GST_STATE_NULL);
        // Wait for state change to complete, but with a timeout to avoid hanging indefinitely
        GstState state;
        GstStateChangeReturn ret = gst_element_get_state(pipeline, &state, nullptr, 2 * GST_SECOND);
        if (ret == GST_STATE_CHANGE_ASYNC) {
            // If still ASYNC after timeout, we proceed regardless (to avoid hang)
            std::cerr << "Warning: Pipeline did not shut down within timeout, forcing teardown." << std::endl;
        }
    }

    // Release bus and pipeline elements
    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }
    if (videoBin) {
        // Optionally, detach video overlay before destroying videoSink (not strictly required)
        if (videoSink && GST_IS_VIDEO_OVERLAY(videoSink)) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink), (guintptr)nullptr);
        }
        gst_object_unref(videoBin);
        videoBin    = nullptr;
        videoSink   = nullptr;
        videoFlip   = nullptr;
        effectFilter = nullptr;
    }
    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    durationNs = 0;
}

void GstPlayer::applyOverlayIfAvailable() {
    if (!videoSink || !surfaceHandle) return;
    if (GST_IS_VIDEO_OVERLAY(videoSink)) {
        // Inform the video sink about the rendering surface (window) to draw on
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink), (guintptr)surfaceHandle);
    }
}

void GstPlayer::setupVideoProcessingBin() {
    // Create an empty bin to hold video processing elements
    videoBin = gst_bin_new("video-processing-bin");
    if (!videoBin) {
        notifyError("Failed to create video processing bin");
        return;
    }
    // Create video flip (rotation) element
    videoFlip = gst_element_factory_make("videoflip", "videoflip");
    if (!videoFlip) {
        notifyError("Failed to create videoflip element");
        return;
    }
    // Create effect filter (always use videobalance for all effects)
    effectFilter = makeEffectFilter(currentEffect);
    if (!effectFilter) {
        notifyError("Failed to create effect filter");
        return;
    }
    // Create video sink (OpenGL/GLES sink for rendering to screen)
    videoSink = makeVideoSink();
    if (!videoSink) {
        notifyError("Failed to create video sink");
        return;
    }
    // Create a converter to ensure compatibility between videoflip and videobalance
    GstElement* converter = gst_element_factory_make("videoconvert", "effect-converter");
    if (!converter) {
        notifyError("Failed to create videoconvert element");
        return;
    }

    // Add and link all video processing elements: flip -> convert -> effect -> sink
    gst_bin_add_many(GST_BIN(videoBin), videoFlip, converter, effectFilter, videoSink, nullptr);
    if (!gst_element_link_many(videoFlip, converter, effectFilter, videoSink, nullptr)) {
        notifyError("Failed to link video processing elements");
        return;
    }

    // Add a ghost pad to the bin to act as a single sink pad for the whole videoBin
    GstPad* sinkPad = gst_element_get_static_pad(videoFlip, "sink");
    gst_element_add_pad(videoBin, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);

    // Initialize effect parameters (e.g., ensure default/none effect has neutral settings)
    applyEffectParams(effectFilter, currentEffect);
}

GstElement* GstPlayer::makeVideoSink() {
    // Use OpenGL video sink for rendering (adjust as needed per platform, e.g., glimagesink for iOS/Android)
    GstElement* sink = gst_element_factory_make("glimagesink", "videosink");
    if (sink) {
        // Maintain aspect ratio
        g_object_set(sink, "force-aspect-ratio", TRUE, nullptr);
    }
    return sink;
}

GstElement* GstPlayer::makeEffectFilter(VisualEffect effect) {
    // Always create a videobalance element. We will adjust its properties for different effects.
    return gst_element_factory_make("videobalance", "effectfilter");
}

void GstPlayer::applyEffectParams(GstElement* filter, VisualEffect effect) {
    if (!filter) return;
    // We expect the filter to be videobalance; confirm the factory name
    GstElementFactory* factory = gst_element_get_factory(filter);
    if (!factory) return;
    const gchar* factoryName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    if (g_strcmp0(factoryName, "videobalance") != 0) {
        return;  // If it's not a videobalance (unexpected), do nothing
    }

    // Set videobalance properties based on the selected effect
    switch (effect) {
        case VisualEffect::NONE:
            // No effect: reset to neutral values
            g_object_set(filter,
                         "brightness", 0.0,
                         "contrast",   1.0,
                         "saturation", 1.0,
                         "hue",        0.0,
                         nullptr);
            break;
        case VisualEffect::GRAINY:
            g_object_set(filter,
                         "brightness", -0.05,
                         "contrast",   0.75,
                         "saturation", 0.9,
                         "hue",        0.0,
                         nullptr);
            break;
        case VisualEffect::GRITTY:
            g_object_set(filter,
                         "brightness", -0.10,
                         "contrast",   1.35,
                         "saturation", 0.55,
                         "hue",        0.0,
                         nullptr);
            break;
        case VisualEffect::HYPER:
            g_object_set(filter,
                         "brightness", 0.10,
                         "contrast",   1.5,
                         "saturation", 1.8,
                         "hue",        0.06,
                         nullptr);
            break;
    }
    // The videobalance element will immediately start applying these new settings to the video stream:contentReference[oaicite:4]{index=4}:contentReference[oaicite:5]{index=5}.
}

void GstPlayer::pollBus() {
    if (!bus) return;
    while (true) {
        GstMessage* msg = gst_bus_pop(bus);
        if (!msg) break;
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                // Handle error messages from pipeline
                GError* err = nullptr;
                gchar* dbg  = nullptr;
                gst_message_parse_error(msg, &err, &dbg);
                if (err) {
                    notifyError(err->message);
                    g_error_free(err);
                }
                if (dbg) {
                    g_free(dbg);
                }
                // On error, stop playback and reset state
                _isPlaying = false;
                _isPlayingInternal = false;
                _stopProgressTimer();
                notifyState(JuceMixPlayerState::STOPPED);
            } break;
            case GST_MESSAGE_EOS: {
                // End-of-stream reached
                _isPlaying = false;
                _isPlayingInternal = false;
                completed = true;
                _stopProgressTimer();
                progress = 1.0f;
                if (onProgressCallback) {
                    onProgressCallback(this, progress);
                }
                // std::cout << "pollBus: onProgressCallback: " << progress << std::endl;
                notifyState(JuceMixPlayerState::COMPLETED);
            } break;
            default:
                // Other messages (STATE_CHANGED, etc.) can be handled if needed
                break;
        }
        gst_message_unref(msg);
    }
}

void GstPlayer::updateProgressFromPipeline() {
    if (!pipeline) return;
    gint64 pos = 0;
    if (!gst_element_query_position(pipeline, GST_FORMAT_TIME, &pos)) {
        return;
    }
    // Update cached duration if possible
    gint64 dur = 0;
    if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur)) {
        durationNs = dur;
    }
    
    if (durationNs > 0) {
        double norm = (double)pos / (double)durationNs;
        // Clamp norm between 0 and 1
        progress = (float) juce::jlimit(0.0, 1.0, norm);
        if (onProgressCallback) {
            onProgressCallback(this, progress);
        }
        std::cout << "updateProgressFromPipeline: " << progress << std::endl;
    }
}

