#include "includes/juce_wrapper_c.h"
#include "GstPlayer.h"
#include "JuceMixPlayer.h"
#include "Logger.h"
#include "Models.h"
#include "UnifiedAVPlayer.h"

void juce_init() { juce::MessageManager::getInstance(); }

void Java_com_rmsl_juce_Native_juceMessageManagerInit() { juce_init(); }

// public method to enable/disable logging
void juce_enableLogs(int enable) { enableLogsValue = enable == 1; }

// MARK: JuceMixPlayer

void *JuceMixPlayer_init() { return new JuceMixPlayer(); }

void JuceMixPlayer_deinit(void *ptr) {
  static_cast<JuceMixPlayer *>(ptr)->dispose();
}

void JuceMixPlayer_play(void *ptr) {
  static_cast<JuceMixPlayer *>(ptr)->play();
}

void JuceMixPlayer_pause(void *ptr) {
  static_cast<JuceMixPlayer *>(ptr)->pause();
}

void JuceMixPlayer_stop(void *ptr) {
  static_cast<JuceMixPlayer *>(ptr)->stop();
}

void JuceMixPlayer_set(void *ptr, const char *json) {
  static_cast<JuceMixPlayer *>(ptr)->setJson(json);
}

void JuceMixPlayer_setSettings(void *ptr, const char *json) {
  static_cast<JuceMixPlayer *>(ptr)->setSettings(json);
}

void JuceMixPlayer_onStateUpdate(void *ptr,
                                 void (*onStateUpdate)(void *ptr,
                                                       const char *)) {
  static_cast<JuceMixPlayer *>(ptr)->onStateUpdateCallback = onStateUpdate;
}

void JuceMixPlayer_onProgress(void *ptr, void (*onProgress)(void *ptr, float)) {
  static_cast<JuceMixPlayer *>(ptr)->onProgressCallback = onProgress;
}

void JuceMixPlayer_onError(void *ptr,
                           void (*onError)(void *ptr, const char *)) {
  static_cast<JuceMixPlayer *>(ptr)->onErrorCallback = onError;
}

float JuceMixPlayer_getDuration(void *ptr) {
  return static_cast<JuceMixPlayer *>(ptr)->getDuration();
}

int JuceMixPlayer_isPlaying(void *ptr) {
  return static_cast<JuceMixPlayer *>(ptr)->isPlaying();
}

void JuceMixPlayer_seek(void *ptr, float value) {
  static_cast<JuceMixPlayer *>(ptr)->seek(value);
}

void JuceMixPlayer_prepareRecorder(void *ptr, const char *file) {
  static_cast<JuceMixPlayer *>(ptr)->prepareRecorder(file);
}

void JuceMixPlayer_startRecorder(void *ptr) {
  static_cast<JuceMixPlayer *>(ptr)->startRecorder();
}

void JuceMixPlayer_stopRecorder(void *ptr) {
  static_cast<JuceMixPlayer *>(ptr)->stopRecorder();
}

void JuceMixPlayer_onRecStateUpdate(void *ptr,
                                    void (*onStateUpdate)(void *ptr,
                                                          const char *)) {
  static_cast<JuceMixPlayer *>(ptr)->onRecStateUpdateCallback = onStateUpdate;
}

void JuceMixPlayer_onRecProgress(void *ptr,
                                 void (*onProgress)(void *ptr, float)) {
  static_cast<JuceMixPlayer *>(ptr)->onRecProgressCallback = onProgress;
}

void JuceMixPlayer_onRecError(void *ptr,
                              void (*onError)(void *ptr, const char *)) {
  static_cast<JuceMixPlayer *>(ptr)->onRecErrorCallback = onError;
}

void JuceMixPlayer_onRecLevel(void *ptr, void (*onLevel)(void *ptr, float)) {
  static_cast<JuceMixPlayer *>(ptr)->onRecLevelCallback = onLevel;
}

void JuceMixPlayer_onDeviceUpdate(void *ptr,
                                  void (*onDeviceUpdate)(void *ptr,
                                                         const char *)) {
  static_cast<JuceMixPlayer *>(ptr)->onDeviceUpdateCallback = onDeviceUpdate;
}

void JuceMixPlayer_setUpdatedDevices(void *ptr, const char *json) {
  static_cast<JuceMixPlayer *>(ptr)->setUpdatedDevices(json);
}

const char *JuceMixPlayer_getDeviceLatencyInfo(void *ptr) {
  return static_cast<JuceMixPlayer *>(ptr)->getDeviceLatencyInfo();
}

void JuceMixPlayer_export(void *ptr, const char *outputPath,
                          void (*completion)(const char *)) {
  return static_cast<JuceMixPlayer *>(ptr)->exportToFile(outputPath,
                                                         completion);
}

// Utility methods
int JuceMixPlayer_fileExists(const char *filePath) {
  juce::File file(filePath);
  return file.exists() ? 1 : 0;
}

// MARK: GstPlayer
void *GstPlayer_init() { return new GstPlayer(); }

void GstPlayer_deinit(void *ptr) { delete static_cast<GstPlayer *>(ptr); }

void GstPlayer_setVideoPath(void *ptr, const char *path) {
  static_cast<GstPlayer *>(ptr)->setVideoPath(path);
}

void GstPlayer_play(void *ptr) { static_cast<GstPlayer *>(ptr)->play(); }

void GstPlayer_pause(void *ptr) { static_cast<GstPlayer *>(ptr)->pause(); }

void GstPlayer_stop(void *ptr) { static_cast<GstPlayer *>(ptr)->stop(); }

void GstPlayer_seek(void *ptr, float normalized) {
  static_cast<GstPlayer *>(ptr)->seek(normalized);
}

int GstPlayer_isPlaying(void *ptr) {
  return static_cast<GstPlayer *>(ptr)->isPlaying();
}

float GstPlayer_getDurationInSecs(void *ptr) {
  return static_cast<GstPlayer *>(ptr)->getDurationInSecs();
}

void GstPlayer_onStateUpdate(void *ptr,
                             void (*callback)(void *, const char *)) {
  static_cast<GstPlayer *>(ptr)->onStateUpdateCallback = callback;
}

void GstPlayer_onProgress(void *ptr, void (*callback)(void *, float)) {
  static_cast<GstPlayer *>(ptr)->onProgressCallback = callback;
}

void GstPlayer_onError(void *ptr, void (*callback)(void *, const char *)) {
  static_cast<GstPlayer *>(ptr)->onErrorCallback = callback;
}

void GstPlayer_setSurfaceHandle(void *ptr, void *nativeSurface) {
  static_cast<GstPlayer *>(ptr)->setSurfaceHandle(nativeSurface);
}

void GstPlayer_setMuteEmbeddedAudio(void *ptr, int mute) {
  static_cast<GstPlayer *>(ptr)->setMuteEmbeddedAudio(mute);
}

void GstPlayer_setRotation(void *ptr, int degrees) {
  static_cast<GstPlayer *>(ptr)->setRotation(degrees);
}

void GstPlayer_setFlip(void *ptr, int method) {
  if (auto *player = static_cast<GstPlayer *>(ptr)) {
    player->setFlip(method);
  }
}
void GstPlayer_setVisualEffect(void *ptr, int effectId) {
  static_cast<GstPlayer *>(ptr)->setVisualEffect(effectId);
}

void GstPlayer_setTrimRange(void *ptr, int startMs, int endMs) {
  static_cast<GstPlayer *>(ptr)->setTrimRange(startMs, endMs);
}

int GstPlayer_getTrimStart(void *ptr) {
  return static_cast<GstPlayer *>(ptr)->getTrimStart();
}

int GstPlayer_getTrimEnd(void *ptr) {
  return static_cast<GstPlayer *>(ptr)->getTrimEnd();
}

void GstPlayer_exportVideo(void *ptr, const char *outputPath,
                           void (*completion)(const char *)) {
  static_cast<GstPlayer *>(ptr)->exportVideo(outputPath, completion);
}

// MARK: UnifiedAVPlayer
void *UnifiedAVPlayer_new() { return new UnifiedAVPlayer(); }
void UnifiedAVPlayer_dispose(void *ptr) {
  if (ptr) {
    reinterpret_cast<UnifiedAVPlayer *>(ptr)->dispose();
  }
}

// Unified playback controls
void UnifiedAVPlayer_play(void *ptr) {
  static_cast<UnifiedAVPlayer *>(ptr)->play();
}

void UnifiedAVPlayer_pause(void *ptr) {
  static_cast<UnifiedAVPlayer *>(ptr)->pause();
}

void UnifiedAVPlayer_stop(void *ptr) {
  static_cast<UnifiedAVPlayer *>(ptr)->stop();
}

void UnifiedAVPlayer_seek(void *ptr, float normalizedPos) {
  static_cast<UnifiedAVPlayer *>(ptr)->seek(normalizedPos);
}

void UnifiedAVPlayer_togglePlayPause(void *ptr) {
  static_cast<UnifiedAVPlayer *>(ptr)->togglePlayPause();
}

// Audio setup (required - primary media)
void UnifiedAVPlayer_setAudioData(void *ptr, const char *json) {
  static_cast<UnifiedAVPlayer *>(ptr)->setAudioData(json);
}

void UnifiedAVPlayer_setAudioSettings(void *ptr, const char *json) {
  static_cast<UnifiedAVPlayer *>(ptr)->setAudioSettings(json);
}

void UnifiedAVPlayer_exportAudio(void *ptr, const char *outputPath,
                                 void (*completion)(const char *)) {
  static_cast<UnifiedAVPlayer *>(ptr)->exportToFile(outputPath, completion);
}

// Video setup (optional - secondary media)
void UnifiedAVPlayer_setVideoPath(void *ptr, const char *path) {
  static_cast<UnifiedAVPlayer *>(ptr)->setVideoPath(path);
}

void UnifiedAVPlayer_setVideoSurfaceHandle(void *ptr, void *handle) {
  static_cast<UnifiedAVPlayer *>(ptr)->setVideoSurfaceHandle(handle);
}

void UnifiedAVPlayer_setVideoRotation(void *ptr, int degrees) {
  static_cast<UnifiedAVPlayer *>(ptr)->setVideoRotation(degrees);
}

void UnifiedAVPlayer_setVideoFlip(void *ptr, int method) {
  if (auto *player = static_cast<UnifiedAVPlayer *>(ptr)) {
    player->setVideoFlip(method);
  }
}
void UnifiedAVPlayer_setVideoVisualEffect(void *ptr, int effectId) {
  static_cast<UnifiedAVPlayer *>(ptr)->setVideoVisualEffect(effectId);
}

void UnifiedAVPlayer_setVideoTrimRange(void *ptr, int startMs, int endMs) {
  static_cast<UnifiedAVPlayer *>(ptr)->setVideoTrimRange(startMs, endMs);
}

void UnifiedAVPlayer_exportVideo(void *ptr, const char *outputPath,
                                 void (*completion)(const char *)) {
  static_cast<UnifiedAVPlayer *>(ptr)->exportVideo(outputPath, completion);
}

// State queries
float UnifiedAVPlayer_getDuration(void *ptr) {
  return static_cast<UnifiedAVPlayer *>(ptr)->getDuration();
}

float UnifiedAVPlayer_getVideoDuration(void *ptr) {
  return static_cast<UnifiedAVPlayer *>(ptr)->getVideoDuration();
}

float UnifiedAVPlayer_getCurrentTime(void *ptr) {
  return static_cast<UnifiedAVPlayer *>(ptr)->getCurrentTime();
}

int UnifiedAVPlayer_isPlaying(void *ptr) {
  return static_cast<UnifiedAVPlayer *>(ptr)->getIsPlaying() ? 1 : 0;
}

const char *UnifiedAVPlayer_getCurrentState(void *ptr) {
  return returnCopyCharDelete(
      static_cast<UnifiedAVPlayer *>(ptr)->getCurrentState());
}

int UnifiedAVPlayer_hasVideoLoaded(void *ptr) {
  return static_cast<UnifiedAVPlayer *>(ptr)->hasVideoLoaded() ? 1 : 0;
}

// Callbacks (unified - driven by audio timeline)
void UnifiedAVPlayer_onProgress(void *ptr, void (*callback)(void *, float)) {
  static_cast<UnifiedAVPlayer *>(ptr)->onProgressCallback = callback;
}

void UnifiedAVPlayer_onStateUpdate(void *ptr,
                                   void (*callback)(void *, const char *)) {
  static_cast<UnifiedAVPlayer *>(ptr)->onStateUpdateCallback = callback;
}

void UnifiedAVPlayer_onError(void *ptr,
                             void (*callback)(void *, const char *)) {
  static_cast<UnifiedAVPlayer *>(ptr)->onErrorCallback = callback;
}

void UnifiedAVPlayer_onDeviceUpdate(void *ptr,
                                    void (*callback)(void *, const char *)) {
  static_cast<UnifiedAVPlayer *>(ptr)->onDeviceUpdateCallback = callback;
}