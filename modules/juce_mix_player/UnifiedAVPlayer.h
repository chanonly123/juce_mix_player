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
  // Singleton instance
  static std::unique_ptr<UnifiedAVPlayer> instance;
  static std::mutex instanceMutex;

  // Player instances
  std::unique_ptr<JuceMixPlayer> audioPlayer;
  std::unique_ptr<GstPlayer> videoPlayer;

  // Synchronization state
  std::atomic<bool> hasVideo{false};
  std::atomic<bool> isPlaying{false};
  std::string videoPath;
  float audioDuration = 0.0f;
  float videoDuration = 0.0f;

  // Drift correction
  const float SYNC_THRESHOLD_MS = 50.0f;      // Max acceptable drift
  const float SYNC_CHECK_INTERVAL_MS = 100.0f; // How often to check sync
  float lastAudioProgress = 0.0f;
  float lastVideoProgress = 0.0f;

  // Thread safety
  juce::CriticalSection lock;

  // Internal state
  JuceMixPlayerState currentState = JuceMixPlayerState::IDLE;

  // Private constructor for singleton
  UnifiedAVPlayer();

  // Internal methods
  void _syncVideoToAudio();
  void _handleAudioStateChange(JuceMixPlayerState state);
  void _handleVideoStateChange(const std::string &state);
  void _handleAudioProgress(float progress);
  void _ensureVideoSyncOnPlay();
  void _logError(const std::string &message);

public:
  // Singleton access
  static UnifiedAVPlayer *getInstance();
  static void destroyInstance();

  // Destructor
  ~UnifiedAVPlayer();

  // Prevent copying
  UnifiedAVPlayer(const UnifiedAVPlayer &) = delete;
  UnifiedAVPlayer &operator=(const UnifiedAVPlayer &) = delete;

  // Unified playback controls
  void play();
  void pause();
  void stop();
  void seek(float normalizedPos); // 0.0 to 1.0
  void togglePlayPause();

  // Audio setup (required - primary media)
  void setAudioData(const char *json);
  void setAudioSettings(const char *json);
  void resetAudioPlayBuffer();

  // Audio export
  void exportToFile(const char *outputFile,
                    std::function<void(const char *)> completion);

  // Video setup (optional - secondary media)
  void setVideoPath(const char *path);
  void setVideoSurfaceHandle(void *handle);
  void setVideoRotation(int degrees);
  void setVideoVisualEffect(int effectId);

  // Video export
  void exportVideo(const char *outputPath,
                   std::function<void(const char *)> completion);

  // State queries
  float getDuration();    // Returns audio duration (master)
  float getCurrentTime(); // Returns audio current time
  bool getIsPlaying();
  std::string getCurrentState();
  bool hasVideoLoaded();

  // Callbacks (unified - driven by audio timeline)
  JuceMixPlayerCallbackFloat onProgressCallback = nullptr;
  JuceMixPlayerCallbackString onStateUpdateCallback = nullptr;
  JuceMixPlayerCallbackString onErrorCallback = nullptr;

  // Device management (audio only)
  JuceMixPlayerCallbackString onDeviceUpdateCallback = nullptr;

  // Timer callback for drift correction
  void timerCallback() override;

  // Cleanup
  void dispose();
};
