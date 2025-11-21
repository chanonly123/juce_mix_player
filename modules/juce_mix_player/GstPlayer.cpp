#include "GstPlayer.h"
#include "Logger.h"
#include <gst/gst.h>
#include <thread>
#include <cstring>
#include <algorithm>

static const guint64 SHUTDOWN_TIMEOUT = 4 * GST_SECOND;

// Constructor: initialize platform
GstPlayer::GstPlayer() : platform(GstPlatform::create()) {
    PRINT("GstPlayer()");
    gstTaskQueue.name = "gstTaskQueue";
    platform->initialize();
}

// Destructor: ensure resources are freed
GstPlayer::~GstPlayer() {
    PRINT("~GstPlayer");
}

void GstPlayer::dispose() {
    PRINT("GstPlayer::dispose");
    _isPlaying = false;
    _isPlayingInternal = false;
    
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&]{
        stop();
        teardownPipeline();
        std::thread thread([&]{
            gstTaskQueue.stopQueue();
            juce::Thread::sleep(2000);
            delete this;
        });
        thread.detach();
    });
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

void GstPlayer::play() {
    PRINT("GstPlayer::play");
    if (!ready || !pipeline) {
        notifyError("Video not ready");
        return;
    }
    gstTaskQueue.async([&]{
        _playInternal();
    });
}

void GstPlayer::_playInternal() {
    if (!_isPlaying) {
        if (completed) {
            completed = false;
            progress  = 0.0f;
            gst_element_seek_simple(
                pipeline, GST_FORMAT_TIME,
                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                0);
        }

        _isPlaying = true;
        _isPlayingInternal = true;
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        _startProgressTimer();
        notifyState(JuceMixPlayerState::PLAYING);
    }
}

void GstPlayer::pause() {
    PRINT("GstPlayer::pause");
    gstTaskQueue.async([&]{
        _pauseInternal(false);
    });
}

void GstPlayer::stop() {
    PRINT("GstPlayer::stop");
    gstTaskQueue.async([&]{
        _pauseInternal(true);
    });
}

void GstPlayer::_pauseInternal(bool stop) {
    if (_isPlaying) {
        _isPlaying = false;
        _isPlayingInternal = false;
        _stopProgressTimer();
        if (pipeline) {
            gst_element_change_state(pipeline, GST_STATE_CHANGE_PLAYING_TO_PAUSED);
        }
    }
    
    if (stop) {
        // TODO:
//          if (pipeline) {
//              gst_element_change_state(pipeline,  GST_STATE_CHANGE_PAUSED_TO_READY);
//          }
// //        progress  = 0.0f;
    //    completed = false;
//         notifyState(JuceMixPlayerState::STOPPED);
        if (!bus) bus = gst_element_get_bus(pipeline);
        gst_bus_set_flushing(bus, TRUE);
       

        gst_element_set_state(pipeline, GST_STATE_NULL);
        GstStateChangeReturn sret =
            gst_element_get_state(pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);

        if (sret != GST_STATE_CHANGE_SUCCESS) {
            notifyError("Failed to stop old pipeline.");
            gst_bus_set_flushing(bus, FALSE);
            return;
        }

        gst_bus_set_flushing(bus, FALSE);
        notifyState(JuceMixPlayerState::STOPPED);
    } else {
        notifyState(JuceMixPlayerState::PAUSED);
    }
}

void GstPlayer::seek(float normalizedPos) {
    std::cout << "GstPlayer::seek: " << normalizedPos << std::endl;
    if (!pipeline || !ready) {
        return;
    }
    
    gstTaskQueue.async([&]{
    
        normalizedPos = std::clamp(normalizedPos, 0.0f, 1.0f);
        _isSeeking = true;
        gint64 targetMs = static_cast<gint64>(durationMs * normalizedPos);
        lastSeekMs = targetMs;
        
        // Target timestamp
        gint64 target = targetMs * 1000000; // convert ms to ns
        std::cout << "GstPlayer::seek: target ns: " << target << std::endl;

        bool result = gst_element_seek(
            pipeline,
            1.0,                     // playback rate
            GST_FORMAT_TIME,
            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
            GST_SEEK_TYPE_SET,       // start type
            target,                  // start position
            GST_SEEK_TYPE_NONE,      // end type
            GST_CLOCK_TIME_NONE      // end position
        );
        
        if (!result) {
            PRINT("Seek failed");
            _isSeeking = false;
            return;
        }
        
        PRINT("SEEK SUCCESS");

        gst_element_get_state(
            pipeline,
            nullptr,
            nullptr,
            GST_CLOCK_TIME_NONE
        );

        // Seek finished – re‑enable progress updates
        _isSeeking = false;
        return;
    });
}

int GstPlayer::isPlaying() {
    return _isPlaying ? 1 : 0;
}

float GstPlayer::getDurationInSecs() {
    return (float)((double)durationMs / 1000.0);
}

gint64 GstPlayer::_getDurationInternal() {
    PRINT("GstPlayer::_getDurationInternal");
    
    if (!pipeline) {
        return 0.0f;
    }
     
    gint64 duration = GST_CLOCK_TIME_NONE;
    if (!gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration) || duration == GST_CLOCK_TIME_NONE) {
        PRINT("GstPlayer::getDuration - no duration");
        return 0.0f;
    }
    // Convert nanoseconds to milliseconds
    return (float)((double)duration / 1e6);;
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


void GstPlayer::exportVideo(const char* outputPath, std::function<void(const char*)> completion) {
    if (!ready || videoPath.empty()) {
        completion("No video loaded");
        return;
    }
    if (_isPlayingInternal) {
        completion("Cannot export while playing. Please pause first.");
        return;
    }

    std::string inputPath = videoPath;
    std::string outputPathStr = outputPath;
    VisualEffect effect = currentEffect;

    int rotationMethod = 0;
    if (videoFlip) {
        g_object_get(videoFlip, "method", &rotationMethod, nullptr);
    }

    gstTaskQueue.async([this, inputPath, outputPathStr, effect, rotationMethod, completion]() {
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
            completion("Failed to create GStreamer export pipeline");
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
                completion(err->message);
                g_error_free(err);
            } else {
                // Success
                completion("");
            }
            gst_message_unref(msg);
        } else {
            completion("Export failed - no message received");
        }

        gst_object_unref(bus);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        delete decodeTargets;
    });
}



void GstPlayer::timerCallback() {
    gstTaskQueue.async([&] {
        pollBus();
        if (!_isSeeking && _isPlayingInternal && _isPlaying && durationMs > 0) {
            gint64 posNs = 0;
            
            if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &posNs)) {
                double posMs = (double)posNs / 1000000.0;  // ns → ms
                progress = (float)(posMs / (double)durationMs);
                progress = std::clamp(progress, 0.0f, 1.0f);
	                if (onProgressCallback) {
	                    double segMs = (double)posNs / 1000000.0;  // ns to ms
	                    double absoluteMs = segMs;
	                    if (durationMs > 0 && lastSeekMs > 0) {
	                        double targetMs = (double) lastSeekMs;
	                        // If GStreamer is reporting a small time that is
	                        // clearly before the last seek target, treat segMs
	                        // as an offset from that target on the full
	                        // timeline (segment-relative position).
	                        if (segMs + 1.0 < targetMs) {
	                            absoluteMs = targetMs + segMs;
	                        }
	                    }
	                    float corrected = (float)(absoluteMs / (double)durationMs);
	                    corrected = std::clamp(corrected, 0.0f, 1.0f);
	                    progress = corrected;
		                onProgressCallback(this, progress);
	                }
            }
        }
    });
}



void GstPlayer::setVideoPath(const char* path)
{
    if (!path || std::strlen(path) == 0) {
        notifyError("Invalid video path");
        return;
    }

    gstTaskQueue.async([this, path] {

        // Stop current playback and reset flags
        _isPlaying = false;
        _isPlayingInternal = false;
        _isSeeking = false;
        _stopProgressTimer();

        teardownPipeline();  // ✅ safe teardown (fixed below)

        videoPath  = path;
        progress   = 0.0f;
        completed  = false;
        durationMs = 0;
        ready      = false;
        lastSeekMs = 0;

        buildPipelineIfNeeded();
        if (!pipeline) {
            notifyError("Failed to initialize video pipeline");
            return;
        }

        // Set URI
        GError* err = nullptr;
        gchar* uri = gst_filename_to_uri(videoPath.c_str(), &err);
        if (err) {
            notifyError(err->message);
            g_error_free(err);
            if (uri) g_free(uri);
            teardownPipeline();
            return;
        }

        g_object_set(pipeline, "uri", uri, nullptr);
        g_free(uri);

        // Go PAUSED and WAIT for preroll
        gst_element_set_state(pipeline, GST_STATE_PAUSED);

        GstStateChangeReturn ret =
            gst_element_get_state(pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);

        if (ret != GST_STATE_CHANGE_SUCCESS &&
            ret != GST_STATE_CHANGE_NO_PREROLL)
        {
            PRINT("Pipeline failed to reach PAUSED");
            teardownPipeline();
            return;
        }

        // Duration is valid after preroll
        durationMs = _getDurationInternal();
        ready = true;

        notifyState(JuceMixPlayerState::READY);
    });
}

void GstPlayer::setSurfaceHandle(void* handle)
{
    gstTaskQueue.async([this, handle] {
        surfaceHandle = handle;
        surfaceHandleAtomic.store((guintptr)handle);
        applyOverlayIfAvailable();
    });
}

void GstPlayer::buildPipelineIfNeeded()
{
    if (pipeline) return;

    pipeline = gst_element_factory_make("playbin", "playbin");
    if (!pipeline) {
        notifyError("Failed to create playbin");
        return;
    }

    setupVideoProcessingBin();
    if (!videoBin) return;

    g_object_set(pipeline, "video-sink", videoBin, nullptr);
    g_object_set(pipeline,
                 "mute",   muteEmbedded ? TRUE : FALSE,
                 "volume", muteEmbedded ? 0.0  : 1.0,
                 nullptr);

    bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, GstPlayer::bus_sync_cb, this, nullptr);
}

void GstPlayer::teardownPipeline()
{
    // Stop pipeline
    if (pipeline) {
        if (bus) gst_bus_set_flushing(bus, TRUE);

        gst_element_set_state(pipeline, GST_STATE_NULL);

        GstState state;
        GstStateChangeReturn ret =
            gst_element_get_state(pipeline, &state, nullptr, SHUTDOWN_TIMEOUT);

        if (ret == GST_STATE_CHANGE_ASYNC) {
            std::cerr << "Warning: Pipeline did not shut down within timeout.\n";
        }
    }

    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }

    // Only unref the BIN. It owns children (videoSink/videoFlip/effect/etc).
    if (videoBin) {
        // Detach overlay handle (safe while sink still alive)
        if (videoSink && GST_IS_VIDEO_OVERLAY(videoSink)) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink), 0);
        }

        gst_object_unref(videoBin);
        videoBin = nullptr;
    }

    // Null out child pointers (they were freed by unref(videoBin))
    videoSink    = nullptr;
    videoFlip    = nullptr;
    effectFilter = nullptr;

    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    durationMs = 0;
}

void GstPlayer::applyOverlayIfAvailable()
{
    if (!videoSink) return;

    guintptr handle = surfaceHandleAtomic.load();
    if (handle == 0) return;

    if (GST_IS_VIDEO_OVERLAY(videoSink)) {
        gst_video_overlay_set_window_handle(
            GST_VIDEO_OVERLAY(videoSink),
            handle
        );
    }
}

void GstPlayer::setupVideoProcessingBin()
{
    videoBin = gst_bin_new("video-processing-bin");
    if (!videoBin) {
        notifyError("Failed to create video processing bin");
        return;
    }

    videoFlip = gst_element_factory_make("videoflip", "videoflip");
    if (!videoFlip) { notifyError("Failed to create videoflip"); gst_object_unref(videoBin); videoBin=nullptr; return; }

    effectFilter = makeEffectFilter(currentEffect);
    if (!effectFilter) { notifyError("Failed to create effect filter"); gst_object_unref(videoBin); videoBin=nullptr; return; }

    videoSink = makeVideoSink();
    if (!videoSink) { notifyError("Failed to create video sink"); gst_object_unref(videoBin); videoBin=nullptr; return; }

    GstElement* converter = gst_element_factory_make("videoconvert", "effect-converter");
    if (!converter) { notifyError("Failed to create videoconvert"); gst_object_unref(videoBin); videoBin=nullptr; return; }

    gst_bin_add_many(GST_BIN(videoBin), videoFlip, converter, effectFilter, videoSink, nullptr);

    if (!gst_element_link_many(videoFlip, converter, effectFilter, videoSink, nullptr)) {
        notifyError("Failed to link video processing elements");
        gst_object_unref(videoBin); videoBin=nullptr;
        return;
    }

    GstPad* sinkPad = gst_element_get_static_pad(videoFlip, "sink");
    gst_element_add_pad(videoBin, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);
    applyEffectParams(effectFilter, currentEffect);
}

GstElement* GstPlayer::makeVideoSink()
{
    GstElement* sink = gst_element_factory_make("glimagesink", "videosink");
    if (sink) {
        g_object_set(sink, "force-aspect-ratio", TRUE, nullptr);
    }
    return sink;
}

GstElement* GstPlayer::makeEffectFilter(VisualEffect /*effect*/)
{
    return gst_element_factory_make("videobalance", "effectfilter");
}

void GstPlayer::applyEffectParams(GstElement* filter, VisualEffect effect) {
    if (!filter) return;
    GstElementFactory* factory = gst_element_get_factory(filter);
    if (!factory) return;
    const gchar* factoryName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    if (g_strcmp0(factoryName, "videobalance") != 0) {
        return;
    }

    switch (effect) {
        case VisualEffect::NONE:
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
                notifyState(JuceMixPlayerState::COMPLETED);
            } break;
            
            default:
                // Other messages (STATE_CHANGED, etc.) can be handled if needed
                break;
        }
        gst_message_unref(msg);
    }
}


GstBusSyncReply GstPlayer::bus_sync_cb(GstBus* /*bus*/, GstMessage* msg, gpointer user_data)
{
    auto* self = static_cast<GstPlayer*>(user_data);
    if (!self) return GST_BUS_PASS;

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT) {
        const GstStructure* s = gst_message_get_structure(msg);
        if (s && gst_structure_has_name(s, "prepare-window-handle")) {

            GstElement* sink = GST_ELEMENT(GST_MESSAGE_SRC(msg));
            if (sink && GST_IS_VIDEO_OVERLAY(sink)) {
                guintptr handle = self->surfaceHandleAtomic.load();

                if (handle != 0) {
                    gst_video_overlay_set_window_handle(
                        GST_VIDEO_OVERLAY(sink),
                        handle
                    );
                }
            }
            return GST_BUS_DROP; // handled; don't forward
        }
    }
    return GST_BUS_PASS;
}
