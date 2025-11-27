//
//  UnifiedAVPlayer.cpp
//
//
//  Created by Animesh on 23/11/25.
//

#include "UnifiedAVPlayer.h"

std::unique_ptr<UnifiedAVPlayer> UnifiedAVPlayer::instance = nullptr;
std::mutex UnifiedAVPlayer::instanceMutex;

UnifiedAVPlayer *UnifiedAVPlayer::getInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex);
  if (!instance) {
    instance.reset(new UnifiedAVPlayer());
  }
  return instance.get();
}

void UnifiedAVPlayer::destroyInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex);
  if (instance) {
    instance->dispose();
    instance.reset();
  }
}

UnifiedAVPlayer::UnifiedAVPlayer() {
  PRINT("UnifiedAVPlayer()");

  audioPlayer = new JuceMixPlayer();
  videoPlayer = new GstPlayer();

  audioPlayer->userContext = this;

  audioPlayer->onProgressCallback = [](void *context, float progress) {
    JuceMixPlayer *player = static_cast<JuceMixPlayer *>(context);
    if (player && player->userContext) {
      UnifiedAVPlayer *self =
          static_cast<UnifiedAVPlayer *>(player->userContext);
      self->_handleAudioProgress(progress);
    }
  };

  audioPlayer->onStateUpdateCallback = [](void *context, const char *state) {
    JuceMixPlayer *player = static_cast<JuceMixPlayer *>(context);
    if (player && player->userContext) {
      UnifiedAVPlayer *self =
          static_cast<UnifiedAVPlayer *>(player->userContext);
      JuceMixPlayerState playerState;
      nlohmann::json j =
          nlohmann::json::parse(std::string("\"") + state + "\"");
      playerState = j.get<JuceMixPlayerState>();
      self->_handleAudioStateChange(playerState);
    }
  };

  audioPlayer->onErrorCallback = [](void *context, const char *error) {
    JuceMixPlayer *player = static_cast<JuceMixPlayer *>(context);
    if (player && player->userContext) {
      UnifiedAVPlayer *self =
          static_cast<UnifiedAVPlayer *>(player->userContext);
      self->_logError(std::string("Audio: ") + error);
    }
  };

  videoPlayer->userContext = this;

  videoPlayer->onStateUpdateCallback = [](void *context, const char *state) {
    GstPlayer *player = static_cast<GstPlayer *>(context);
    if (player && player->userContext) {
      UnifiedAVPlayer *self =
          static_cast<UnifiedAVPlayer *>(player->userContext);
      self->_handleVideoStateChange(std::string(state));
    }
  };

  videoPlayer->onErrorCallback = [](void *context, const char *error) {
    GstPlayer *player = static_cast<GstPlayer *>(context);
    if (player && player->userContext) {
      UnifiedAVPlayer *self =
          static_cast<UnifiedAVPlayer *>(player->userContext);
      self->_logError(std::string("Video: ") + error);
    }
  };

  startTimer(SYNC_CHECK_INTERVAL_MS);

  PRINT("UnifiedAVPlayer initialized");
}

void UnifiedAVPlayer::dispose() {
  PRINT("UnifiedAVPlayer::dispose");
  isPlaying = false;
  isDisposed = true;
  stopTimer();

  if (audioPlayer) {
    audioPlayer->dispose();
  }

  if (videoPlayer) {
    videoPlayer->dispose();
  }
}

UnifiedAVPlayer::~UnifiedAVPlayer() { PRINT("~UnifiedAVPlayer"); }

// MARK: Unified Playback Controls
void UnifiedAVPlayer::play() {
  PRINT("UnifiedAVPlayer::play");
  const juce::ScopedLock scopedLock(lock);

  if (!audioPlayer) {
    _logError("Audio player not initialized");
    return;
  }

  audioPlayer->play();

  if (hasVideo && videoPlayer) {
    _ensureVideoSyncOnPlay();
  }

  isPlaying = true;
}

void UnifiedAVPlayer::pause() {
  PRINT("UnifiedAVPlayer::pause");
  const juce::ScopedLock scopedLock(lock);
  isPlaying = false;

  if (audioPlayer) {
    audioPlayer->pause();
  }

  if (hasVideo && videoPlayer) {
    videoPlayer->pause();
  }
}

void UnifiedAVPlayer::stop() {
  PRINT("UnifiedAVPlayer::stop");
  if (isDisposed)
    return;

  const juce::ScopedLock scopedLock(lock);
  isPlaying = false;

  if (audioPlayer) {
    audioPlayer->stop();
  }

  if (hasVideo && videoPlayer) {
    videoPlayer->stop();
  }

  lastAudioProgress = 0.0f;
  lastVideoProgress = 0.0f;
}

void UnifiedAVPlayer::seek(float normalizedPos) {
  PRINT("UnifiedAVPlayer::seek: " << normalizedPos);
  if (isDisposed)
    return;
  const juce::ScopedLock scopedLock(lock);

  float value = std::min(1.0f, std::max(normalizedPos, 0.0f));

  if (audioPlayer) {
    audioPlayer->seek(value);
  }

  if (hasVideo && videoPlayer) {
    float videoPos = value;
    if (videoDuration > 0 && audioDuration > 0) {
      if (videoDuration > audioDuration) {
        videoPos = value * (audioDuration / videoDuration);
      }
    }
    videoPlayer->seek(videoPos);
  }
}

void UnifiedAVPlayer::togglePlayPause() {
  if (isPlaying) {
    pause();
  } else {
    play();
  }
}

// MARK: Audio Setup

void UnifiedAVPlayer::setAudioData(const char *json) {
  PRINT("UnifiedAVPlayer::setAudioData");
  const juce::ScopedLock scopedLock(lock);

  if (!audioPlayer) {
    _logError("Audio player not initialized");
    return;
  }

  audioPlayer->setJson(json);

  audioDuration = audioPlayer->getDuration();
  PRINT("Audio duration: " << audioDuration << " seconds");
}

void UnifiedAVPlayer::setAudioSettings(const char *json) {
  PRINT("UnifiedAVPlayer::setAudioSettings");
  const juce::ScopedLock scopedLock(lock);

  if (!audioPlayer) {
    _logError("Audio player not initialized");
    return;
  }

  audioPlayer->setSettings(json);
}

void UnifiedAVPlayer::resetAudioPlayBuffer() {
  PRINT("UnifiedAVPlayer::resetAudioPlayBuffer");
  const juce::ScopedLock scopedLock(lock);

  if (audioPlayer) {
    audioPlayer->resetPlayBuffer();
  }
}

void UnifiedAVPlayer::exportToFile(
    const char *outputFile, std::function<void(const char *)> completion) {
  PRINT("UnifiedAVPlayer::exportToFile");
  const juce::ScopedLock scopedLock(lock);

  if (!audioPlayer) {
    completion("Audio player not initialized");
    return;
  }

  audioPlayer->exportToFile(outputFile, completion);
}

// MARK: Video Setup

void UnifiedAVPlayer::setVideoPath(const char *path) {
  PRINT("UnifiedAVPlayer::setVideoPath: " << path);
  const juce::ScopedLock scopedLock(lock);

  if (!videoPlayer) {
    _logError("Video player not initialized");
    return;
  }

  // Reset audio to initial state when video is changed
  if (audioPlayer && audioDuration > 0) {
    PRINT("Video changed - resetting audio to initial state");
    _resetAudioToInitialState();
  }

  videoPath = std::string(path);
  videoPlayer->setVideoPath(path);
}

void UnifiedAVPlayer::setVideoSurfaceHandle(void *handle) {
  PRINT("UnifiedAVPlayer::setVideoSurfaceHandle");
  const juce::ScopedLock scopedLock(lock);

  if (!videoPlayer) {
    _logError("Video player not initialized");
    return;
  }

  videoPlayer->setSurfaceHandle(handle);
}

void UnifiedAVPlayer::setVideoRotation(int degrees) {
  PRINT("UnifiedAVPlayer::setVideoRotation: " << degrees);
  const juce::ScopedLock scopedLock(lock);

  if (!hasVideo || !videoPlayer) {
    return;
  }

  videoPlayer->setRotation(degrees);
}

void UnifiedAVPlayer::setVideoVisualEffect(int effectId) {
  PRINT("UnifiedAVPlayer::setVideoVisualEffect: " << effectId);
  const juce::ScopedLock scopedLock(lock);

  if (!hasVideo || !videoPlayer) {
    return;
  }

  videoPlayer->setVisualEffect(effectId);
}

void UnifiedAVPlayer::exportVideo(
    const char *outputPath, std::function<void(const char *)> completion) {
  PRINT("UnifiedAVPlayer::exportVideo");
  const juce::ScopedLock scopedLock(lock);

  if (!hasVideo || !videoPlayer) {
    completion("No video loaded");
    return;
  }

  videoPlayer->exportVideo(outputPath, completion);
}

// MARK: State Queries

float UnifiedAVPlayer::getDuration() {
  if (audioPlayer) {
    return audioPlayer->getDuration();
  }
  return 0.0f;
}

float UnifiedAVPlayer::getCurrentTime() {
  if (audioPlayer) {
    return audioPlayer->getCurrentTime();
  }
  return 0.0f;
}

bool UnifiedAVPlayer::getIsPlaying() { return isPlaying; }

std::string UnifiedAVPlayer::getCurrentState() {
  if (audioPlayer) {
    return audioPlayer->getCurrentState();
  }
  return JuceMixPlayerState_toString(JuceMixPlayerState::IDLE);
}

bool UnifiedAVPlayer::hasVideoLoaded() { return hasVideo; }

// MARK: Internal Synchronization

void UnifiedAVPlayer::_handleAudioProgress(float progress) {
  lastAudioProgress = progress;
  bool shouldBlack = false;

  if (hasVideo && videoDuration > 0.0f && audioDuration > 0.0f &&
      videoDuration < audioDuration) {
    const float marginSeconds = 0.15f;

    float audioTime = progress * audioDuration;
    float thresholdTime = videoDuration - marginSeconds;
    if (thresholdTime < 0.0f)
      thresholdTime = 0.0f;
    shouldBlack = (audioTime >= thresholdTime);
  }

  if (shouldBlack != isVideoAfterEndForPlayback) {
    isVideoAfterEndForPlayback = shouldBlack;
    _updateVideoBlackOverlayForPlayback(shouldBlack);
  }

  if (onProgressCallback) {
    onProgressCallback(this, progress);
  }
}

void UnifiedAVPlayer::_handleAudioStateChange(JuceMixPlayerState state) {
  PRINT("UnifiedAVPlayer::_handleAudioStateChange: "
        << JuceMixPlayerState_toString(state));
  currentState = state;

  if (state == JuceMixPlayerState::READY && audioPlayer) {
    audioDuration = audioPlayer->getDuration();
    PRINT("Audio ready, duration: " << audioDuration);
  }

  // When audio completes, stop the video player to prevent it from
  // continuing to play when video duration > audio duration
  if (state == JuceMixPlayerState::COMPLETED) {
    PRINT("Audio completed - stopping video player");
    isPlaying = false;

    if (hasVideo && videoPlayer) {
      videoPlayer->pause();
    }
  }

  if (onStateUpdateCallback) {
    onStateUpdateCallback(
        this, returnCopyCharDelete(JuceMixPlayerState_toString(state)));
  }
}

void UnifiedAVPlayer::_handleVideoStateChange(const std::string &state) {
  PRINT("UnifiedAVPlayer::_handleVideoStateChange: " << state);

  if (state == "READY" && videoPlayer) {
    videoDuration = videoPlayer->getDurationInSecs();
    hasVideo = true;
    isVideoAfterEndForPlayback = false;
    PRINT("Video ready, duration: " << videoDuration);

    videoPlayer->setMuteEmbeddedAudio(1);

    if (audioDuration > 0) {
      if (videoDuration > audioDuration) {
        PRINT("WARNING: Video duration ("
              << videoDuration << "s) > audio duration (" << audioDuration
              << "s). Video will be trimmed.");
      } else if (videoDuration < audioDuration) {
        PRINT("WARNING: Video duration ("
              << videoDuration << "s) < audio duration (" << audioDuration
              << "s). Video will show black padding after end.");
      }
    }
  }

  if (state == "STOPPED" || state == "ERROR") {
    hasVideo = false;
  }
}

void UnifiedAVPlayer::_ensureVideoSyncOnPlay() {
  if (!hasVideo || !videoPlayer || !audioPlayer) {
    return;
  }

  float audioPos = audioPlayer->getCurrentTime();
  float audioDur = audioPlayer->getDuration();

  if (audioDur <= 0) {
    videoPlayer->play();
    return;
  }

  float normalized = audioPos / audioDur;

  float videoPos = normalized;
  if (videoDuration > 0 && audioDuration > 0) {
    if (videoDuration > audioDuration) {
      videoPos = normalized * (audioDuration / videoDuration);
    }
  }

  videoPlayer->seek(videoPos);
  juce::Thread::sleep(50);
  videoPlayer->play();
}

void UnifiedAVPlayer::_updateVideoBlackOverlayForPlayback(bool enable) {
  PRINT("UnifiedAVPlayer::_updateVideoBlackOverlayForPlayback");
  if (!hasVideo || !videoPlayer) {
    return;
  }

  videoPlayer->setBlackOverlayEnabled(enable ? 1 : 0);
}

void UnifiedAVPlayer::_syncVideoToAudio() {
  if (!hasVideo || !isPlaying || !videoPlayer || !audioPlayer) {
    return;
  }

  float audioTime = audioPlayer->getCurrentTime();
  float audioDur = audioPlayer->getDuration();

  if (audioDur <= 0) {
    return;
  }

  float audioProgress = audioTime / audioDur;

  float progressDiff = std::abs(audioProgress - lastAudioProgress);
  if (progressDiff > (SYNC_THRESHOLD_MS / (audioDur * 1000.0f))) {
    float videoPos = audioProgress;
    if (videoDuration > 0 && audioDuration > 0) {
      if (videoDuration > audioDuration) {
        videoPos = audioProgress * (audioDuration / videoDuration);
      }
    }

    PRINT("Applying corrective video seek to: " << videoPos);
    videoPlayer->seek(videoPos);
  }
}

void UnifiedAVPlayer::_logError(const std::string &message) {
  PRINT("ERROR: " << message);
  if (onErrorCallback) {
    onErrorCallback(this, returnCopyCharDelete(message));
  }
}

// MARK: Timer Callback for Drift Correction

void UnifiedAVPlayer::timerCallback() {
  if (isPlaying && hasVideo) {
    _syncVideoToAudio();
  }
}

// MARK: Media Reset Helpers

void UnifiedAVPlayer::_resetAudioToInitialState() {
  PRINT("UnifiedAVPlayer::_resetAudioToInitialState");
  if (!audioPlayer) {
    return;
  }

  audioPlayer->pause();
  audioPlayer->seek(0.0f);
  lastAudioProgress = 0.0f;
}
