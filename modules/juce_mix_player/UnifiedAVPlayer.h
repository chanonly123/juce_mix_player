//
//  UnifiedAVPlayer.h
//
//
//  Created by Animesh on 23/11/25.
//

#pragma once

#include "GstPlayer.h"
#include "JuceMixPlayer.h"
#include "Logger.h"
#include "Models.h"
#include <atomic>
#include <memory>
#include <mutex>

/**
 * UnifiedAVPlayer coordinates synchronized playback of audio (via
 * JuceMixPlayer) and video (via GstPlayer). Audio is primary/master, video is
 * secondary.
 *
 * Key features:
 * - Audio duration is master timeline
 * - Video is optional and always muted
 * - Event-driven synchronization with drift correction
 * - Single unified playback controls
 */
class UnifiedAVPlayer : private juce::Timer {
private:
  JuceMixPlayer *audioPlayer;
  GstPlayer *videoPlayer;

  std::atomic<bool> hasVideo{false};
  std::atomic<bool> isPlaying{false};
  std::atomic<bool> isDisposed{false};
  std::string videoPath;
  float audioDuration = 0.0f;
  float videoDuration = 0.0f;
  bool isVideoAfterEndForPlayback = false;

  const float SYNC_THRESHOLD_MS = 80.0f;
  const float SYNC_CHECK_INTERVAL_MS = 150.0f;
  float lastAudioProgress = 0.0f;
  float lastVideoProgress = 0.0f;

  juce::CriticalSection lock;
  JuceMixPlayerState currentState = JuceMixPlayerState::IDLE;

  void _syncVideoToAudio();
  void _handleAudioStateChange(JuceMixPlayerState state);
  void _handleVideoStateChange(const std::string &state);
  void _handleAudioProgress(float progress);
  void _updateVideoBlackOverlayForPlayback(bool enable);
  void _ensureVideoSyncOnPlay();
  void _logError(const std::string &message);
  void _resetAudioToInitialState();

public:
  UnifiedAVPlayer();
  ~UnifiedAVPlayer();

  UnifiedAVPlayer(const UnifiedAVPlayer &) = delete;
  UnifiedAVPlayer &operator=(const UnifiedAVPlayer &) = delete;

  void play();
  void pause();
  void stop();
  void seek(float normalizedPos); // 0.0 to 1.0
  void togglePlayPause();

  void setAudioData(const char *json);
  void setAudioSettings(const char *json);
  void exportToFile(const char *outputFile,
                    std::function<void(const char *)> completion);

  void setVideoPath(const char *path);
  void setVideoSurfaceHandle(void *handle);
  void setVideoRotation(int degrees);
  void setVideoFlip(int method);
  void setVideoVisualEffect(int effectId);

  void exportVideo(const char *outputPath,
                   std::function<void(const char *)> completion);

  float getDuration();
  float getVideoDuration();
  float getCurrentTime();
  bool getIsPlaying();
  std::string getCurrentState();
  bool hasVideoLoaded();

  JuceMixPlayerCallbackFloat onProgressCallback = nullptr;
  JuceMixPlayerCallbackString onStateUpdateCallback = nullptr;
  JuceMixPlayerCallbackString onErrorCallback = nullptr;
  JuceMixPlayerCallbackString onDeviceUpdateCallback = nullptr;

  void timerCallback() override;
  void dispose();
};
