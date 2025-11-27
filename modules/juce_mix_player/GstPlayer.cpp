#include "GstPlayer.h"
#include "Logger.h"
#include <algorithm>
#include <cstring>
#include <gst/gst.h>
#include <thread>

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
    _isPlaying = false;
    _isPlayingInternal = false;
    stop();
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
    if (_isCompleted) {
      _isCompleted = false;
      _progress = 0.0f;
      gst_element_seek_simple(
          pipeline, GST_FORMAT_TIME,
          GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0);
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
      gst_element_change_state(pipeline, GST_STATE_CHANGE_PLAYING_TO_PAUSED);
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
    // normalizedPos = std::clamp(normalizedPos, 0.0f, 1.0f);
    float seekPos = std::clamp(normalizedPos, 0.0f, 1.0f);
    _isSeeking = true;

    // Reset completed flag to allow seeking after EOS
    _isCompleted = false;

    if (_durationMs <= 0) {
      PRINT("ERROR: Cannot seek - duration not set or invalid");
      _isSeeking = false;
      return;
    }

    gint64 targetMs = static_cast<gint64>(_durationMs * seekPos);
    _lastSeekMs = targetMs;

    // Target timestamp
    gint64 target = targetMs * 1000000; // convert ms to ns
    std::cout << "GstPlayer::seek: target ns: " << target << std::endl;

    bool result = gst_element_seek(
        pipeline,
        1.0, // playback rate
        GST_FORMAT_TIME,
        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET,  // start type
        target,             // start position
        GST_SEEK_TYPE_NONE, // end type
        GST_CLOCK_TIME_NONE // end position
    );

    if (!result) {
      PRINT("Seek failed");
      _isSeeking = false;
      return;
    }

    PRINT("SEEK SUCCESS");

    gst_element_get_state(pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);

    // Seek finished – re‑enable progress updates
    _isSeeking = false;
    return;
  });
}

int GstPlayer::isPlaying() { return _isPlaying ? 1 : 0; }

float GstPlayer::getDurationInSecs() {
  return (float)((double)_durationMs / 1000.0);
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
  // Convert nanoseconds to milliseconds
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

  // Control the volume element if it exists
  if (audioVolume) {
    g_object_set(audioVolume, "mute", _muteEmbedded ? TRUE : FALSE, "volume",
                 _muteEmbedded ? 0.0 : 1.0, nullptr);
    PRINT(std::string("Audio mute set to: ") +
          (_muteEmbedded ? "true" : "false"));
  } else {
    PRINT("Warning: audioVolume element not available");
  }
}

void GstPlayer::setRotation(int degree) {
  if (degree != 0 && degree != 90 && degree != 180 && degree != 270) {
    notifyError("Invalid rotation angle. Use 0, 90, 180, or 270 degrees.");
    return;
  }
  if (_currentRotation == degree) {
    return;
  }
  _currentRotation = degree;

  if (videoFlip) {
    int method = 0;
    switch (degree) {
    case 0:
      method = 0;
      break;
    case 90:
      method = 1;
      break;
    case 180:
      method = 2;
      break;
    case 270:
      method = 3;
      break;
    }
    g_object_set(videoFlip, "method", method, nullptr);
  }
  std::cout << "ROTATION APPLIED: " << degree << std::endl;
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
    notifyError("Invalid effect ID. Use 0 (NONE), 1 (GRAINY), 2 (GRITTY), or 3 "
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

  int rotationMethod = 0;
  if (videoFlip) {
    g_object_get(videoFlip, "method", &rotationMethod, nullptr);
  }

  gstTaskQueue.async([this, inputPath, outputPathStr, effect, rotationMethod,
                      completion]() {
    GstElement *pipeline = gst_pipeline_new("export_pipeline");
    GstElement *src = gst_element_factory_make("filesrc", "src");
    GstElement *decodebin = gst_element_factory_make("decodebin", "decodebin");
    GstElement *videoQueue = gst_element_factory_make("queue", "video_queue");
    GstElement *audioQueue = gst_element_factory_make("queue", "audio_queue");
    GstElement *balance = gst_element_factory_make("videobalance", "balance");
    GstElement *flip = gst_element_factory_make("videoflip", "flip");
    GstElement *vconv = gst_element_factory_make("videoconvert", "vconv");
    GstElement *x264enc = gst_element_factory_make("x264enc", "x264enc");
    GstElement *h264parse = gst_element_factory_make("h264parse", "h264parse");
    GstElement *aconv = gst_element_factory_make("audioconvert", "aconv");
    GstElement *aresample =
        gst_element_factory_make("audioresample", "aresample");
    GstElement *aacenc = gst_element_factory_make("voaacenc", "aacenc");
    GstElement *aacparse = gst_element_factory_make("aacparse", "aacparse");
    GstElement *mp4mux = gst_element_factory_make("mp4mux", "mux");
    GstElement *sink = gst_element_factory_make("filesink", "sink");

    if (!pipeline || !src || !decodebin || !videoQueue || !audioQueue ||
        !balance || !flip || !vconv || !x264enc || !h264parse || !aconv ||
        !aresample || !aacenc || !aacparse || !mp4mux || !sink) {
      completion("Failed to create GStreamer export pipeline");
      return;
    }

    g_object_set(src, "location", inputPath.c_str(), nullptr);
    g_object_set(sink, "location", outputPathStr.c_str(), "sync", FALSE,
                 nullptr);
    g_object_set(flip, "method", rotationMethod, nullptr);
    applyEffectParams(_currentEffect);

    gst_bin_add_many(GST_BIN(pipeline), src, decodebin, videoQueue, balance,
                     flip, vconv, x264enc, h264parse, audioQueue, aconv,
                     aresample, aacenc, aacparse, mp4mux, sink, nullptr);

    gst_element_link(src, decodebin);
    gst_element_link_many(videoQueue, balance, flip, vconv, x264enc, h264parse,
                          nullptr);
    gst_element_link_many(audioQueue, aconv, aresample, aacenc, aacparse,
                          nullptr);
    gst_element_link(mp4mux, sink);

    auto *decodeTargets =
        new std::pair<GstElement *, GstElement *>(videoQueue, audioQueue);
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(onPadAdded),
                     decodeTargets);

    GstPad *videoSinkPad = gst_element_request_pad_simple(mp4mux, "video_0");
    GstPad *audioSinkPad = gst_element_request_pad_simple(mp4mux, "audio_0");
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

void GstPlayer::timerCallback() {
  gstTaskQueue.async([&] {
    pollBus();
    if (!_isSeeking && _isPlayingInternal && _isPlaying && _durationMs > 0) {
      gint64 posNs = 0;

      if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &posNs)) {
        double posMs = (double)posNs / 1000000.0; // ns → ms
        _progress = (float)(posMs / (double)_durationMs);
        _progress = std::clamp(_progress, 0.0f, 1.0f);
        if (onProgressCallback) {
          double segMs = (double)posNs / 1000000.0; // ns to ms
          double absoluteMs = segMs;
          if (_durationMs > 0 && _lastSeekMs > 0) {
            double targetMs = (double)_lastSeekMs;
            // If GStreamer is reporting a small time that is
            // clearly before the last seek target, treat segMs
            // as an offset from that target on the full
            // timeline (segment-relative position).
            if (segMs + 1.0 < targetMs) {
              absoluteMs = targetMs + segMs;
            }
          }
          float corrected = (float)(absoluteMs / (double)_durationMs);
          corrected = std::clamp(corrected, 0.0f, 1.0f);
          _progress = corrected;
          onProgressCallback(this, _progress);
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

    // Check if file exists
    juce::File videoFile(pathStr);
    if (!videoFile.existsAsFile()) {
      notifyError("Video file does not exist");
      return;
    }

    // Stop current playback if active
    if (_isPlaying || _isPlayingInternal) {
      _pauseInternal(true);
    }

    // Clean up old pipeline
    teardownPipeline();

    // Store new path
    _videoPath = pathStr;
    _isReady = false;
    _isCompleted = false;
    _progress = 0.0f;
    _blackOverlayEnabled = false;

    // Build new pipeline
    buildPipeline();

    if (!pipeline) {
      notifyError("Failed to create pipeline");
      return;
    }

    // Query duration
    _durationMs = _getDurationInternal();
    PRINT("Duration: " + std::to_string(_durationMs) + " ms");

    // Mark as ready
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

  // Apply overlay if pipeline and video sink are ready
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

  // Create main pipeline
  pipeline = gst_pipeline_new("video_player");
  if (!pipeline) {
    notifyError("Failed to create pipeline");
    return;
  }

  // Create source element
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

  // Create video processing bin (includes effects, rotation, and sink)
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

  // Create audio processing chain with volume control
  // audioconvert → volume → autoaudiosink
  audioConvert = gst_element_factory_make("audioconvert", "audio_convert");
  audioVolume = gst_element_factory_make("volume", "audio_volume");
  audioSink = gst_element_factory_make("autoaudiosink", "audio_sink");

  if (!audioConvert || !audioVolume || !audioSink) {
    PRINT("Warning: Failed to create audio elements, continuing without audio");
    // Clean up any that were created
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
    // Set initial mute state
    g_object_set(audioVolume, "mute", _muteEmbedded ? TRUE : FALSE, "volume",
                 _muteEmbedded ? 0.0 : 1.0, nullptr);
  }

  // Add elements to pipeline
  gst_bin_add_many(GST_BIN(pipeline), source, decodebin, videoBin, nullptr);
  if (audioConvert && audioVolume && audioSink) {
    gst_bin_add_many(GST_BIN(pipeline), audioConvert, audioVolume, audioSink,
                     nullptr);

    // Link audio chain
    if (!gst_element_link_many(audioConvert, audioVolume, audioSink, nullptr)) {
      PRINT("Warning: Failed to link audio chain");
    }
  }

  // Link source to decodebin
  if (!gst_element_link(source, decodebin)) {
    notifyError("Failed to link source to decodebin");
    teardownPipeline();
    return;
  }

  // Connect decodebin pad-added signal for dynamic linking
  // Audio pads will connect to audioConvert (first element in audio chain)
  decodebinPadData =
      new std::pair<GstElement *, GstElement *>(videoBin, audioConvert);
  g_signal_connect(decodebin, "pad-added", G_CALLBACK(onPadAdded),
                   decodebinPadData);

  // Get bus for message handling
  bus = gst_element_get_bus(pipeline);

  // Apply overlay if surface handle is already set
  if (surfaceHandle) {
    applyOverlay();
  }

  // Set pipeline to PAUSED state to preroll
  GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    notifyError("Failed to set pipeline to PAUSED state");
    teardownPipeline(); // teardownPipeline will clean up decodebinPadData
    return;
  }

  // Wait for preroll to complete
  ret = gst_element_get_state(pipeline, nullptr, nullptr, 5 * GST_SECOND);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    notifyError("Pipeline preroll failed");
    teardownPipeline(); // teardownPipeline will clean up decodebinPadData
    return;
  }

  PRINT("Pipeline built successfully");
}

void GstPlayer::teardownPipeline() {
  PRINT("GstPlayer::teardownPipeline");

  // Stop progress timer
  _stopProgressTimer();

  if (!pipeline) {
    // Nothing to tear down
    return;
  }

  // Set pipeline to NULL state
  gst_element_set_state(pipeline, GST_STATE_NULL);

  // Wait for state change with timeout
  GstStateChangeReturn ret =
      gst_element_get_state(pipeline, nullptr, nullptr, SHUTDOWN_TIMEOUT);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    PRINT("Warning: Pipeline failed to transition to NULL state");
  }

  // Unref bus if it exists
  if (bus) {
    gst_object_unref(bus);
    bus = nullptr;
  }

  // Unref pipeline (this will also unref all child elements)
  gst_object_unref(pipeline);

  // Clear all element pointers (they're already unreffed via pipeline)
  pipeline = nullptr;
  source = nullptr;
  decodebin = nullptr;
  videoBin = nullptr;
  videoSink = nullptr;
  videoFlip = nullptr;
  videoBalance = nullptr;
  videoQueue = nullptr;
  videoConvert = nullptr;
  audioConvert = nullptr;
  audioVolume = nullptr;
  audioSink = nullptr;

  // Clean up dynamically allocated pad data
  if (decodebinPadData) {
    delete decodebinPadData;
    decodebinPadData = nullptr;
  }

  // Reset state
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

  // Create a bin to hold all video processing elements
  videoBin = gst_bin_new("video_processing");
  if (!videoBin) {
    notifyError("Failed to create video processing bin");
    return;
  }

  // Create video processing elements
  videoQueue = gst_element_factory_make("queue", "video_queue");
  GstElement *videoConvert1 =
      gst_element_factory_make("videoconvert", "video_convert1");
  GstElement *videoScale =
      gst_element_factory_make("videoscale", "video_scale");
  videoBalance = gst_element_factory_make("videobalance", "video_balance");
  videoFlip = gst_element_factory_make("videoflip", "video_flip");
  videoConvert = gst_element_factory_make("videoconvert", "video_convert2");

  // Create video sink - glimagesink for iOS with OpenGL rendering
  videoSink = gst_element_factory_make("glimagesink", "video_sink");

  if (!videoQueue || !videoConvert1 || !videoScale || !videoBalance ||
      !videoFlip || !videoConvert || !videoSink) {
    notifyError("Failed to create video processing elements");
    if (videoBin)
      gst_object_unref(videoBin);
    videoBin = nullptr;
    return;
  }

  // Configure video sink properties
  g_object_set(videoSink, "sync",
               TRUE, // Sync to clock for proper playback speed
               nullptr);

  // Add all elements to the bin
  gst_bin_add_many(GST_BIN(videoBin), videoQueue, videoConvert1, videoScale,
                   videoBalance, videoFlip, videoConvert, videoSink, nullptr);

  // Link all elements in the video processing chain
  if (!gst_element_link_many(videoQueue, videoConvert1, videoScale,
                             videoBalance, videoFlip, videoConvert, videoSink,
                             nullptr)) {
    notifyError("Failed to link video processing elements");
    gst_object_unref(videoBin);
    videoBin = nullptr;
    return;
  }

  // Create a ghost pad for the bin's sink (input from decodebin)
  GstPad *sinkPad = gst_element_get_static_pad(videoQueue, "sink");
  GstPad *ghostSink = gst_ghost_pad_new("sink", sinkPad);
  gst_element_add_pad(videoBin, ghostSink);
  gst_object_unref(sinkPad);

  // Set initial effect parameters (NONE)
  applyEffectParams(VisualEffect::NONE);

  // Set initial rotation (0 degrees)
  g_object_set(videoFlip, "method", 0, nullptr); // 0 = identity (no rotation)

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

  // If black overlay is enabled, force a fully black frame regardless of
  // the currently selected visual effect.
  if (_blackOverlayEnabled) {
    g_object_set(videoBalance, "brightness", -1.0, "contrast", 0.0,
                 "saturation", 0.0, "hue", 0.0, nullptr);
    return;
  }

  switch (effect) {
  case VisualEffect::NONE:
    g_object_set(videoBalance, "brightness", 0.0, "contrast", 1.0, "saturation",
                 1.0, "hue", 0.0, nullptr);
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
      // Handle error messages from pipeline
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
      _isCompleted = true;
      _stopProgressTimer();
      _progress = 1.0f;
      if (onProgressCallback) {
        onProgressCallback(this, _progress);
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
