#include "GstPlayer.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <gst/gst.h>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const guint64 SHUTDOWN_TIMEOUT = 4 * GST_SECOND;

// Constructor: initialize platform
GstPlayer::GstPlayer() : platform(GstPlatform::create()) {
    PRINT("GstPlayer()");
    gstTaskQueue.name = "gstTaskQueue";
    platform->initialize();
}

GstPlayer::~GstPlayer() { PRINT("~GstPlayer"); }

void GstPlayer::dispose() {
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([&] {
        PRINT("GstPlayer::dispose");
        pause();
        teardownPipeline();
        std::thread thread([&] {
            gstTaskQueue.stopQueue();
            juce::Thread::sleep(5000);
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

void GstPlayer::notifyError(const char *message) {
    if (onErrorCallback) {
        onErrorCallback(this, returnCopyCharDelete(message));
    }
}

void GstPlayer::play() {
    PRINT("GstPlayer::play");
    if (!_isReady || !pipeline) {
        notifyError("Video not ready");
        return;
    }
    gstTaskQueue.async([&] { _playInternal(); });
}

void GstPlayer::_playInternal() {
    if (!_isPlaying) {
        if (_isCompleted || _progress == 0.0f) {
            _isCompleted = false;
            
            // LOGIC FIX: Determine if we start in padding or video based on
            // _trimStartMs (Virtual Time) Virtual Time 0.._activePaddingMs is
            // Padding. Virtual Time _activePaddingMs..End is Video.
            
            if (_activePaddingMs > 0 && _trimStartMs < _activePaddingMs) {
                // Start in Padding Phase
                _isInPadding = true;
                _paddingPositionMs = (double)_trimStartMs;
                
                PRINT("Starting playback in PADDING phase from " +
                      std::to_string(_paddingPositionMs) + "ms");
                
                // Pipeline should be ready at 0 (start of video file)
                gst_element_seek_simple(
                                        pipeline, GST_FORMAT_TIME,
                                        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                        0);
                
                // Ensure pipeline doesn't progress
                gst_element_set_state(pipeline, GST_STATE_PAUSED);
                
            } else {
                // Start in Video Phase
                _isInPadding = false;
                
                // Calculate position in Video File (Virtual - PaddingOffset)
                gint64 startVideoMs = (gint64)(_trimStartMs - _activePaddingMs);
                if (startVideoMs < 0)
                    startVideoMs = 0;
                
                PRINT("Starting playback in VIDEO phase from " +
                      std::to_string(startVideoMs) + "ms");
                
                gint64 startNs = startVideoMs * 1000000;
                gst_element_seek_simple(
                                        pipeline, GST_FORMAT_TIME,
                                        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                        startNs);
                
                gst_element_set_state(pipeline, GST_STATE_PLAYING);
            }
        } else {
            // Resuming from pause
            if (!_isInPadding) {
                gst_element_set_state(pipeline, GST_STATE_PLAYING);
            } else {
                PRINT("Resuming playback in PADDING phase");
                // Ensure state matches (might be needed if paused)
            }
        }
        
        _isPlaying = true;
        _isPlayingInternal = true;
        
        _startProgressTimer();
        notifyState(JuceMixPlayerState::PLAYING);
    }
}

void GstPlayer::pause() {
    PRINT("GstPlayer::pause");
    gstTaskQueue.async([&] { _pauseInternal(false); });
}

void GstPlayer::stop() {
    PRINT("GstPlayer::stop");
    gstTaskQueue.async([&] { _pauseInternal(true); });
}

void GstPlayer::_pauseInternal(bool stop) {
    if (_isPlaying) {
        _isPlaying = false;
        _isPlayingInternal = false;
        _stopProgressTimer();
        if (pipeline) {
            gst_element_change_state(pipeline,
                                     GST_STATE_CHANGE_PLAYING_TO_PAUSED);
        }
    }
    
    // Reset completed flag when pausing to allow seeking/playing again
    _isCompleted = false;
    
    if (stop) {
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            GstStateChangeReturn sret = gst_element_get_state(
                                                              pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);
            
            if (sret != GST_STATE_CHANGE_SUCCESS) {
                notifyError("Failed to stop old pipeline.");
                if (bus) {
                    gst_bus_set_flushing(bus, FALSE);
                }
                return;
            }
        }
        
        notifyState(JuceMixPlayerState::STOPPED);
    } else {
        notifyState(JuceMixPlayerState::PAUSED);
    }
}

void GstPlayer::seek(float normalizedPos) {
    std::cout << "GstPlayer::seek: " << normalizedPos << std::endl;
    if (!pipeline || !_isReady) {
        return;
    }
    
    gstTaskQueue.async([&, normalizedPos] {
        float seekPos = std::clamp(normalizedPos, 0.0f, 1.0f);
        _isSeeking = true;
        _isCompleted = false;
        bool wasPlaying = _isPlaying;
        
        if (_durationMs <= 0 && _activePaddingMs <= 0) {
            PRINT("ERROR: Cannot seek - duration not set or invalid");
            _isSeeking = false;
            return;
        }
        
        // Logic Upgrade: Use Virtual Timeline
        // _trimStartMs and _trimEndMs are Virtual Points
        // _activePaddingMs is the boundary
        
        double trimmedDurationMs = (_trimEndMs - _trimStartMs);
        if (trimmedDurationMs <= 0)
            trimmedDurationMs = 1.0; // Avoid divide by zero
        
        double targetVirtualMs = _trimStartMs + (trimmedDurationMs * seekPos);
        
        gint64 targetNs = 0;
        
        if (_activePaddingMs > 0 && targetVirtualMs < _activePaddingMs) {
            // Seek into padding
            PRINT("Seeking into padding: " + std::to_string(targetVirtualMs) +
                  "ms");
            
            _isInPadding = true;
            _paddingPositionMs = targetVirtualMs;
            
            // Pipeline goes to 0 (Frozen Start)
            targetNs = 0;
            _lastSeekMs = 0;
            
            // Ensure pipeline is paused if we are in padding
            if (pipeline) {
                gst_element_set_state(pipeline, GST_STATE_PAUSED);
            }
            
            // We need to perform the seek on pipeline to frame 0 to be sure
            gst_element_seek(
                             pipeline, 1.0, GST_FORMAT_TIME,
                             (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                             GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
            
        } else {
            // Seek into video
            _isInPadding = false;
            
            double targetVideoMs = targetVirtualMs - _activePaddingMs;
            if (targetVideoMs < 0)
                targetVideoMs = 0;
            
            targetNs = (gint64)(targetVideoMs * 1000000);
            _lastSeekMs = (gint64)targetVideoMs;
            
            PRINT("Seeking into video: " + std::to_string(targetVideoMs) +
                  "ms (Virtual: " + std::to_string(targetVirtualMs) + ")");
            
            bool result = gst_element_seek(
                                           pipeline, 1.0, GST_FORMAT_TIME,
                                           (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                                           GST_SEEK_TYPE_SET, targetNs, GST_SEEK_TYPE_NONE,
                                           GST_CLOCK_TIME_NONE);
            
            if (!result) {
                PRINT("Seek failed");
                _isSeeking = false;
                return;
            }
            
            // If playing, ensure pipeline is playing (might have been in
            // padding before)
            if (wasPlaying && pipeline) {
                gst_element_set_state(pipeline, GST_STATE_PLAYING);
            }
        }
        
        PRINT("SEEK SUCCESS");
        
        gst_element_get_state(pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);
        
        if (!wasPlaying && pipeline && !_isInPadding) {
            gst_element_set_state(pipeline, GST_STATE_PAUSED);
            gst_element_get_state(pipeline, nullptr, nullptr,
                                  GST_CLOCK_TIME_NONE);
        }
        
        _isSeeking = false;
        return;
    });
}

int GstPlayer::isPlaying() { return _isPlaying ? 1 : 0; }

float GstPlayer::getDurationInSecs() {
    if (_activePaddingMs > 0 && _durationMs > 0) {
        return (float)((double)(_activePaddingMs + _durationMs) / 1000.0);
    }
    return (float)((double)_durationMs / 1000.0);
}

void GstPlayer::setPadding(int durationMs, int enabled) {
    _configPaddingMs = durationMs;
    _configPaddingEnabled = (enabled != 0);
    PRINT("GstPlayer::setPadding: " + std::to_string(durationMs) +
          "ms, enabled: " + std::to_string(enabled));
}

gint64 GstPlayer::_getDurationInternal() {
    PRINT("GstPlayer::_getDurationInternal");
    
    if (!pipeline) {
        return 0.0f;
    }
    
    gint64 duration = GST_CLOCK_TIME_NONE;
    if (!gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration) ||
        duration == GST_CLOCK_TIME_NONE) {
        PRINT("GstPlayer::getDuration - no duration");
        return 0.0f;
    }
    return (float)((double)duration / 1e6);
    ;
}

void GstPlayer::setProgressUpdateInterval(float seconds) {
    if (seconds <= 0)
        return;
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
    _muteEmbedded = (mute != 0);
    
    if (audioVolume) {
        g_object_set(audioVolume, "mute", _muteEmbedded ? TRUE : FALSE,
                     "volume", _muteEmbedded ? 0.0 : 1.0, nullptr);
        PRINT(std::string("Audio mute set to: ") +
              (_muteEmbedded ? "true" : "false"));
    } else {
        PRINT("Warning: audioVolume element not available");
    }
}

void GstPlayer::setRotation(int degree) {
    // Normalize degree to 0-360 range
    int normalizedDegree = degree % 360;
    if (normalizedDegree < 0) {
        normalizedDegree += 360;
    }
    
    if (_currentRotation == normalizedDegree) {
        return;
    }
    _currentRotation = normalizedDegree;
    
    if (videoRotate) {
        double radians = (double)normalizedDegree * (M_PI / 180.0);
        std::cout << "ROTATION APPLIED: " << normalizedDegree << " - "
        << radians << std::endl;
        g_object_set(videoRotate, "angle", radians, nullptr);
    }
}

void GstPlayer::setFlip(int method) {
    // Clamp method to valid range 0-8
    int validMethod = std::max(0, std::min(8, method));
    
    if (_currentFlipMethod == validMethod) {
        return;
    }
    _currentFlipMethod = validMethod;
    
    if (videoFlip) {
        g_object_set(videoFlip, "method", validMethod, nullptr);
        std::cout << "FLIP METHOD APPLIED: " << validMethod << std::endl;
    }
}

void GstPlayer::setVisualEffect(int effectId) {
    VisualEffect newEffect;
    switch (effectId) {
        case 0:
            newEffect = VisualEffect::NONE;
            break;
        case 1:
            newEffect = VisualEffect::GRAINY;
            break;
        case 2:
            newEffect = VisualEffect::GRITTY;
            break;
        case 3:
            newEffect = VisualEffect::HYPER;
            break;
        default:
            notifyError(
                        "Invalid effect ID. Use 0 (NONE), 1 (GRAINY), 2 (GRITTY), or 3 "
                        "(HYPER).");
            return;
    }
    if (_currentEffect == newEffect) {
        return;
    }
    _currentEffect = newEffect;
    
    if (videoBalance) {
        applyEffectParams(_currentEffect);
    }
    std::cout << "VIDEO FILTER APPLIED: " << effectId << std::endl;
}

void GstPlayer::setBlackOverlayEnabled(int enabled) {
    bool newValue = (enabled != 0);
    if (_blackOverlayEnabled == newValue) {
        return;
    }
    
    _blackOverlayEnabled = newValue;
    
    if (videoBalance) {
        applyEffectParams(_currentEffect);
    }
}

static void onPadAdded(GstElement *src, GstPad *pad, gpointer data) {
    auto elems = static_cast<std::pair<GstElement *, GstElement *> *>(data);
    GstCaps *caps = gst_pad_get_current_caps(pad);
    const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    GstPad *sinkPad = nullptr;
    
    if (g_str_has_prefix(name, "video/")) {
        sinkPad = gst_element_get_static_pad(elems->first, "sink");
    } else if (g_str_has_prefix(name, "audio/")) {
        sinkPad = gst_element_get_static_pad(elems->second, "sink");
    }
    
    if (sinkPad && !gst_pad_is_linked(sinkPad)) {
        gst_pad_link(pad, sinkPad);
    }
    
    if (caps)
        gst_caps_unref(caps);
    if (sinkPad)
        gst_object_unref(sinkPad);
}

void GstPlayer::exportVideo(const char *outputPath,
                            std::function<void(const char *)> completion) {
    if (!_isReady || _videoPath.empty()) {
        completion("No video loaded");
        return;
    }
    if (_isPlayingInternal) {
        completion("Cannot export while playing. Please pause first.");
        return;
    }
    
    std::string inputPath = _videoPath;
    std::string outputPathStr = outputPath;
    VisualEffect effect = _currentEffect;
    
    double rotationAngle = 0.0;
    if (videoRotate) {
        g_object_get(videoRotate, "angle", &rotationAngle, nullptr);
    }
    
    int flipMethod = 0;
    if (videoFlip) {
        g_object_get(videoFlip, "method", &flipMethod, nullptr);
    }
    
    gstTaskQueue.async([this, inputPath, outputPathStr, effect, rotationAngle,
                        flipMethod, completion]() {
        GstElement *pipeline = gst_pipeline_new("export_pipeline");
        GstElement *src = gst_element_factory_make("filesrc", "src");
        GstElement *decodebin =
        gst_element_factory_make("decodebin", "decodebin");
        GstElement *videoQueue =
        gst_element_factory_make("queue", "video_queue");
        GstElement *audioQueue =
        gst_element_factory_make("queue", "audio_queue");
        GstElement *balance =
        gst_element_factory_make("videobalance", "balance");
        GstElement *flip = gst_element_factory_make("videoflip", "flip");
        GstElement *rotate = gst_element_factory_make("rotate", "rotate");
        GstElement *vconv = gst_element_factory_make("videoconvert", "vconv");
        GstElement *x264enc = gst_element_factory_make("x264enc", "x264enc");
        GstElement *h264parse =
        gst_element_factory_make("h264parse", "h264parse");
        GstElement *aconv = gst_element_factory_make("audioconvert", "aconv");
        GstElement *aresample =
        gst_element_factory_make("audioresample", "aresample");
        GstElement *aacenc = gst_element_factory_make("voaacenc", "aacenc");
        GstElement *aacparse = gst_element_factory_make("aacparse", "aacparse");
        GstElement *mp4mux = gst_element_factory_make("mp4mux", "mux");
        GstElement *sink = gst_element_factory_make("filesink", "sink");
        
        if (!pipeline || !src || !decodebin || !videoQueue || !audioQueue ||
            !balance || !flip || !rotate || !vconv || !x264enc || !h264parse ||
            !aconv || !aresample || !aacenc || !aacparse || !mp4mux || !sink) {
            completion("Failed to create GStreamer export pipeline");
            return;
        }
        
        g_object_set(src, "location", inputPath.c_str(), nullptr);
        g_object_set(sink, "location", outputPathStr.c_str(), "sync", FALSE,
                     nullptr);
        g_object_set(rotate, "angle", rotationAngle, nullptr);
        g_object_set(flip, "method", flipMethod, nullptr);
        applyEffectParams(_currentEffect);
        
        gst_bin_add_many(GST_BIN(pipeline), src, decodebin, videoQueue, balance,
                         flip, rotate, vconv, x264enc, h264parse, audioQueue,
                         aconv, aresample, aacenc, aacparse, mp4mux, sink,
                         nullptr);
        
        gst_element_link(src, decodebin);
        gst_element_link_many(videoQueue, balance, flip, rotate, vconv, x264enc,
                              h264parse, nullptr);
        gst_element_link_many(audioQueue, aconv, aresample, aacenc, aacparse,
                              nullptr);
        gst_element_link(mp4mux, sink);
        
        auto *decodeTargets =
        new std::pair<GstElement *, GstElement *>(videoQueue, audioQueue);
        g_signal_connect(decodebin, "pad-added", G_CALLBACK(onPadAdded),
                         decodeTargets);
        
        GstPad *videoSinkPad =
        gst_element_request_pad_simple(mp4mux, "video_0");
        GstPad *audioSinkPad =
        gst_element_request_pad_simple(mp4mux, "audio_0");
        GstPad *videoSrcPad = gst_element_get_static_pad(h264parse, "src");
        GstPad *audioSrcPad = gst_element_get_static_pad(aacparse, "src");
        gst_pad_link(videoSrcPad, videoSinkPad);
        gst_pad_link(audioSrcPad, audioSinkPad);
        gst_object_unref(videoSinkPad);
        gst_object_unref(audioSinkPad);
        gst_object_unref(videoSrcPad);
        gst_object_unref(audioSrcPad);
        
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        
        GstBus *bus = gst_element_get_bus(pipeline);
        GstMessage *msg = gst_bus_timed_pop_filtered(
                                                     bus, GST_CLOCK_TIME_NONE,
                                                     (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        
        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError *err;
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

void GstPlayer::timerCallback() { // In timerCallback
    gstTaskQueue.async([&] {
        pollBus();
        
        // Padding Logic
        if (_isPlaying && _isInPadding) {
            double deltaMs = progressUpdateIntervalSec * 1000.0;
            _paddingPositionMs += deltaMs;
            
            if (_paddingPositionMs >= _activePaddingMs) {
                // Padding Switchover
                _isInPadding = false;
                _paddingPositionMs = _activePaddingMs;
                PRINT("Padding finished, starting video...");
                gst_element_set_state(pipeline, GST_STATE_PLAYING);
            }
            
            // Calculate progress during padding using Virtual Timeline
            double trimmedDurationMs = _trimEndMs - _trimStartMs;
            if (trimmedDurationMs > 0) {
                double relativePosMs = _paddingPositionMs - _trimStartMs;
                _progress = (float)(relativePosMs / trimmedDurationMs);
                _progress = std::clamp(_progress, 0.0f, 1.0f);
                
                if (onProgressCallback)
                    onProgressCallback(this, _progress);
            }
            return;
        }
        
        if (!_isSeeking && _isPlayingInternal && _isPlaying &&
            _durationMs > 0) {
            gint64 posNs = 0;
            
            if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &posNs)) {
                double posMs = (double)posNs / 1e6;
                
                // NEW LOGIC: Calculate Virtual Time
                double currentVirtualMs = _activePaddingMs + posMs;
                double trimmedDurationMs = _trimEndMs - _trimStartMs;
                
                if (trimmedDurationMs > 0) {
                    double relativePosMs = currentVirtualMs - _trimStartMs;
                    
                    _progress = (float)(relativePosMs / trimmedDurationMs);
                    _progress = std::clamp(_progress, 0.0f, 1.0f);
                    
                    // Check Completion based on Virtual Time
                    if (currentVirtualMs >= _trimEndMs) {
                        _isPlaying = false;
                        _isPlayingInternal = false;
                        _isCompleted = true;
                        _stopProgressTimer();
                        _progress = 1.0f;
                        if (pipeline) {
                            gst_element_set_state(pipeline, GST_STATE_PAUSED);
                        }
                        if (onProgressCallback) {
                            onProgressCallback(this, _progress);
                        }
                        notifyState(JuceMixPlayerState::COMPLETED);
                        return;
                    }
                    
                    if (onProgressCallback) {
                        onProgressCallback(this, _progress);
                    }
                }
            }
        }
    });
}

void GstPlayer::setVideoPath(const char *path) {
    if (!path || std::strlen(path) == 0) {
        notifyError("Invalid video path");
        return;
    }
    
    gstTaskQueue.async([this, pathStr = std::string(path)] {
        PRINT("GstPlayer::setVideoPath: " + pathStr);
        
        juce::File videoFile(pathStr);
        if (!videoFile.existsAsFile()) {
            notifyError("Video file does not exist");
            return;
        }
        
        if (_isPlaying || _isPlayingInternal) {
            _pauseInternal(true);
        }
        
        teardownPipeline();
        
        _videoPath = pathStr;
        _isReady = false;
        _isCompleted = false;
        _progress = 0.0f;
        _blackOverlayEnabled = false;
        
        buildPipeline();
        
        if (!pipeline) {
            notifyError("Failed to create pipeline");
            return;
        }
        
        _durationMs = _getDurationInternal();
        PRINT("Duration: " + std::to_string(_durationMs) + " ms");
        
        // Lock in padding settings
        _activePaddingMs = _configPaddingEnabled ? _configPaddingMs : 0;
        _isInPadding = (_activePaddingMs > 0);
        _paddingPositionMs = 0.0;
        
        if (_activePaddingMs > 0) {
            PRINT("Padding active: " + std::to_string(_activePaddingMs) +
                  " ms");
            // Seek to 0 to ensure we are at start frame
            gst_element_seek_simple(
                                    pipeline, GST_FORMAT_TIME,
                                    GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0);
        }
        
        _trimStartMs = 0.0;
        _trimEndMs = (double)_durationMs;
        
        _isReady = true;
        notifyState(JuceMixPlayerState::READY);
    });
}

void GstPlayer::setSurfaceHandle(void *handle) {
    PRINT("GstPlayer::setSurfaceHandle | this=" +
          juce::String::toHexString(
                                    reinterpret_cast<juce::pointer_sized_int>(this)));
    surfaceHandleAtomic.store(reinterpret_cast<guintptr>(handle),
                              std::memory_order_release);
    surfaceHandle = handle;
    
    gstTaskQueue.async([this] {
        if (videoSink && pipeline) {
            applyOverlay();
        }
    });
}

void GstPlayer::buildPipeline() {
    PRINT("GstPlayer::buildPipeline");
    
    if (_videoPath.empty()) {
        notifyError("No video path set");
        return;
    }
    
    pipeline = gst_pipeline_new("video_player");
    if (!pipeline) {
        notifyError("Failed to create pipeline");
        return;
    }
    
    source = gst_element_factory_make("filesrc", "source");
    if (!source) {
        notifyError("Failed to create filesrc");
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return;
    }
    g_object_set(source, "location", _videoPath.c_str(), nullptr);
    
    // Create decodebin for automatic demuxing and decoding
    decodebin = gst_element_factory_make("decodebin", "decoder");
    if (!decodebin) {
        notifyError("Failed to create decodebin");
        gst_object_unref(source);
        gst_object_unref(pipeline);
        source = nullptr;
        pipeline = nullptr;
        return;
    }
    
    setupVideoProcessingBin();
    if (!videoBin) {
        notifyError("Failed to create video processing bin");
        gst_object_unref(decodebin);
        gst_object_unref(source);
        gst_object_unref(pipeline);
        decodebin = nullptr;
        source = nullptr;
        pipeline = nullptr;
        return;
    }
    
    audioConvert = gst_element_factory_make("audioconvert", "audio_convert");
    audioVolume = gst_element_factory_make("volume", "audio_volume");
    audioSink = gst_element_factory_make("autoaudiosink", "audio_sink");
    
    if (!audioConvert || !audioVolume || !audioSink) {
        PRINT("Warning: Failed to create audio elements, continuing without "
              "audio");
        if (audioConvert)
            gst_object_unref(audioConvert);
        if (audioVolume)
            gst_object_unref(audioVolume);
        if (audioSink)
            gst_object_unref(audioSink);
        audioConvert = nullptr;
        audioVolume = nullptr;
        audioSink = nullptr;
    } else {
        g_object_set(audioVolume, "mute", _muteEmbedded ? TRUE : FALSE,
                     "volume", _muteEmbedded ? 0.0 : 1.0, nullptr);
    }
    
    gst_bin_add_many(GST_BIN(pipeline), source, decodebin, videoBin, nullptr);
    if (audioConvert && audioVolume && audioSink) {
        gst_bin_add_many(GST_BIN(pipeline), audioConvert, audioVolume,
                         audioSink, nullptr);
        
        if (!gst_element_link_many(audioConvert, audioVolume, audioSink,
                                   nullptr)) {
            PRINT("Warning: Failed to link audio chain");
        }
    }
    
    if (!gst_element_link(source, decodebin)) {
        notifyError("Failed to link source to decodebin");
        teardownPipeline();
        return;
    }
    
    decodebinPadData =
    new std::pair<GstElement *, GstElement *>(videoBin, audioConvert);
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(onPadAdded),
                     decodebinPadData);
    
    bus = gst_element_get_bus(pipeline);
    
    if (surfaceHandle) {
        applyOverlay();
    }
    
    GstStateChangeReturn ret =
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        notifyError("Failed to set pipeline to PAUSED state");
        teardownPipeline();
        return;
    }
    
    ret = gst_element_get_state(pipeline, nullptr, nullptr, 5 * GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        notifyError("Pipeline preroll failed");
        teardownPipeline();
    }
    
    PRINT("Pipeline built successfully");
}

void GstPlayer::teardownPipeline() {
    PRINT("GstPlayer::teardownPipeline");
    
    _stopProgressTimer();
    if (!pipeline) {
        return;
    }
    
    gst_element_set_state(pipeline, GST_STATE_NULL);
    GstStateChangeReturn ret =
    gst_element_get_state(pipeline, nullptr, nullptr, SHUTDOWN_TIMEOUT);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        PRINT("Warning: Pipeline failed to transition to NULL state");
    }
    
    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }
    
    gst_object_unref(pipeline);
    
    pipeline = nullptr;
    source = nullptr;
    decodebin = nullptr;
    videoBin = nullptr;
    videoSink = nullptr;
    videoRotate = nullptr;
    videoFlip = nullptr;
    videoBalance = nullptr;
    videoQueue = nullptr;
    videoConvert = nullptr;
    audioConvert = nullptr;
    audioVolume = nullptr;
    audioSink = nullptr;
    
    if (decodebinPadData) {
        delete decodebinPadData;
        decodebinPadData = nullptr;
    }
    
    _isReady = false;
    _durationMs = 0;
    _progress = 0.0f;
    
    PRINT("Pipeline teardown complete");
}

void GstPlayer::applyOverlay() {
    PRINT("GstPlayer::applyOverlay");
    
    if (!videoSink) {
        PRINT("Warning: No video sink available for overlay");
        return;
    }
    
    if (!GST_IS_VIDEO_OVERLAY(videoSink)) {
        PRINT("Warning: Video sink does not support GstVideoOverlay interface");
        return;
    }
    
    void *handle = reinterpret_cast<void *>(
                                            surfaceHandleAtomic.load(std::memory_order_acquire));
    
    if (!handle) {
        PRINT("Warning: No surface handle set");
        return;
    }
    
    PRINT("Applying video overlay with handle");
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink),
                                        reinterpret_cast<guintptr>(handle));
    
    PRINT("Video overlay applied successfully");
}

void GstPlayer::setupVideoProcessingBin() {
    PRINT("GstPlayer::setupVideoProcessingBin");
    
    videoBin = gst_bin_new("video_processing");
    if (!videoBin) {
        notifyError("Failed to create video processing bin");
        return;
    }
    
    videoQueue = gst_element_factory_make("queue", "video_queue");
    GstElement *videoConvert1 =
    gst_element_factory_make("videoconvert", "video_convert1");
    GstElement *videoScale =
    gst_element_factory_make("videoscale", "video_scale");
    videoBalance = gst_element_factory_make("videobalance", "video_balance");
    videoFlip = gst_element_factory_make("videoflip", "video_flip");
    videoRotate = gst_element_factory_make("rotate", "video_rotate");
    videoConvert = gst_element_factory_make("videoconvert", "video_convert2");
    
    videoSink = gst_element_factory_make("glimagesink", "video_sink");
    
    if (!videoQueue || !videoConvert1 || !videoScale || !videoBalance ||
        !videoFlip || !videoRotate || !videoConvert || !videoSink) {
        notifyError("Failed to create video processing elements");
        if (videoBin)
            gst_object_unref(videoBin);
        videoBin = nullptr;
        return;
    }
    
    g_object_set(videoSink, "sync", TRUE, nullptr);
    
    gst_bin_add_many(GST_BIN(videoBin), videoQueue, videoConvert1, videoScale,
                     videoBalance, videoFlip, videoRotate, videoConvert,
                     videoSink, nullptr);
    
    if (!gst_element_link_many(videoQueue, videoConvert1, videoScale,
                               videoBalance, videoFlip, videoRotate,
                               videoConvert, videoSink, nullptr)) {
        notifyError("Failed to link video processing elements");
        gst_object_unref(videoBin);
        videoBin = nullptr;
        return;
    }
    
    GstPad *sinkPad = gst_element_get_static_pad(videoQueue, "sink");
    GstPad *ghostSink = gst_ghost_pad_new("sink", sinkPad);
    gst_element_add_pad(videoBin, ghostSink);
    gst_object_unref(sinkPad);
    applyEffectParams(VisualEffect::NONE);
    
    g_object_set(videoRotate, "angle", 0.0, nullptr);
    g_object_set(videoFlip, "method", 0, nullptr);
    
    PRINT("Video processing bin setup complete");
}

void GstPlayer::applyEffectParams(VisualEffect effect) {
    if (!videoBalance)
        return;
    
    GstElementFactory *factory = gst_element_get_factory(videoBalance);
    if (!factory)
        return;
    
    const gchar *factoryName =
    gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    
    if (g_strcmp0(factoryName, "videobalance") != 0) {
        return;
    }
    
    if (_blackOverlayEnabled) {
        g_object_set(videoBalance, "brightness", -1.0, "contrast", 0.0,
                     "saturation", 0.0, "hue", 0.0, nullptr);
        return;
    }
    
    switch (effect) {
        case VisualEffect::NONE:
            g_object_set(videoBalance, "brightness", 0.0, "contrast", 1.0,
                         "saturation", 1.0, "hue", 0.0, nullptr);
            break;
        case VisualEffect::GRAINY:
            g_object_set(videoBalance, "brightness", -0.05, "contrast", 0.75,
                         "saturation", 0.9, "hue", 0.0, nullptr);
            break;
        case VisualEffect::GRITTY:
            g_object_set(videoBalance, "brightness", -0.10, "contrast", 1.35,
                         "saturation", 0.55, "hue", 0.0, nullptr);
            break;
        case VisualEffect::HYPER:
            g_object_set(videoBalance, "brightness", 0.10, "contrast", 1.5,
                         "saturation", 1.8, "hue", 0.06, nullptr);
            break;
    }
}

void GstPlayer::pollBus() {
    if (!bus)
        return;
    while (true) {
        GstMessage *msg = gst_bus_pop(bus);
        if (!msg)
            break;
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err = nullptr;
                gchar *dbg = nullptr;
                gst_message_parse_error(msg, &err, &dbg);
                if (err) {
                    notifyError(err->message);
                    g_error_free(err);
                }
                if (dbg) {
                    g_free(dbg);
                }
                _isPlaying = false;
                _isPlayingInternal = false;
                _stopProgressTimer();
                notifyState(JuceMixPlayerState::STOPPED);
            } break;
                
            case GST_MESSAGE_EOS: {
                _isPlaying = false;
                _isPlayingInternal = false;
                _isCompleted = true;
                _stopProgressTimer();
                _progress = 1.0f;
                if (onProgressCallback) {
                    onProgressCallback(this, _progress);
                }
                notifyState(JuceMixPlayerState::COMPLETED);
            } break;
                
            default:
                break;
        }
        gst_message_unref(msg);
    }
}

void GstPlayer::setTrimRange(int startMs, int endMs) {
    PRINT("GstPlayer::setTrimRange: start=" + std::to_string(startMs) +
          "ms, end=" + std::to_string(endMs) + "ms");
    
    if (_durationMs <= 0) {
        PRINT("Warning: Cannot set trim range - no video loaded");
        return;
    }

    // Use Virtual Total Duration (ConfigPadding + VideoDuration)
    int totalVirtualDuration = (int)_durationMs + _activePaddingMs;

    int validStart = std::max(0, std::min(startMs, totalVirtualDuration));
    int validEnd = std::max(validStart, std::min(endMs, totalVirtualDuration));

    _trimStartMs = validStart;
    _trimEndMs = validEnd;

    PRINT("Trim range set: " + std::to_string(_trimStartMs) + "ms to " +
          std::to_string(_trimEndMs) + "ms");
}

int GstPlayer::getTrimStart() { return _trimStartMs; }

int GstPlayer::getTrimEnd() { return _trimEndMs; }
