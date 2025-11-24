#pragma once

#ifdef __cplusplus
#define EXPORT_C_FUNC                                                          \
  extern "C" __attribute__((visibility("default"))) __attribute__((used))
#else
#define EXPORT_C_FUNC extern
#endif

EXPORT_C_FUNC void juce_init();
EXPORT_C_FUNC void Java_com_rmsl_juce_Native_juceMessageManagerInit();
EXPORT_C_FUNC void juce_enableLogs(int enable);

EXPORT_C_FUNC void *JuceMixPlayer_init();
EXPORT_C_FUNC void JuceMixPlayer_deinit(void *ptr);

EXPORT_C_FUNC void JuceMixPlayer_play(void *ptr);
EXPORT_C_FUNC void JuceMixPlayer_pause(void *ptr);
EXPORT_C_FUNC void JuceMixPlayer_stop(void *ptr);

EXPORT_C_FUNC void JuceMixPlayer_set(void *ptr, const char *json);

EXPORT_C_FUNC void JuceMixPlayer_setSettings(void *ptr, const char *json);

EXPORT_C_FUNC void JuceMixPlayer_onStateUpdate(
    void *ptr, void (*JuceMixPlayerCallbackString)(void *, const char *));

/// callback with progress value range 0 to 1
EXPORT_C_FUNC void JuceMixPlayer_onProgress(void *ptr,
                                            void (*onProgress)(void *, float));

EXPORT_C_FUNC void JuceMixPlayer_onError(void *ptr,
                                         void (*onError)(void *, const char *));

/// value returns time in seconds
EXPORT_C_FUNC float JuceMixPlayer_getDuration(void *ptr);

/// returns 1 if playing else 0
EXPORT_C_FUNC int JuceMixPlayer_isPlaying(void *ptr);

/// value range 0 to 1
EXPORT_C_FUNC void JuceMixPlayer_seek(void *ptr, float value);

// MARK: Recorder

EXPORT_C_FUNC void JuceMixPlayer_prepareRecorder(void *ptr, const char *file);

EXPORT_C_FUNC void JuceMixPlayer_startRecorder(void *ptr);

EXPORT_C_FUNC void JuceMixPlayer_stopRecorder(void *ptr);

EXPORT_C_FUNC void
JuceMixPlayer_onRecStateUpdate(void *ptr,
                               void (*onStateUpdate)(void *ptr, const char *));

EXPORT_C_FUNC void
JuceMixPlayer_onRecProgress(void *ptr, void (*onProgress)(void *ptr, float));

EXPORT_C_FUNC void
JuceMixPlayer_onRecError(void *ptr, void (*onError)(void *ptr, const char *));

EXPORT_C_FUNC void JuceMixPlayer_onRecLevel(void *ptr,
                                            void (*onLevel)(void *ptr, float));

EXPORT_C_FUNC void
JuceMixPlayer_onDeviceUpdate(void *ptr,
                             void (*onDeviceUpdate)(void *ptr, const char *));

EXPORT_C_FUNC void JuceMixPlayer_setUpdatedDevices(void *ptr, const char *json);

EXPORT_C_FUNC const char *JuceMixPlayer_getDeviceLatencyInfo(void *ptr);

EXPORT_C_FUNC void JuceMixPlayer_export(void *ptr, const char *outputPath,
                                        void (*completion)(const char *));

EXPORT_C_FUNC int JuceMixPlayer_fileExists(const char *filePath);

// MARK: GstPlayer (Phase 1 skeleton)
EXPORT_C_FUNC void *GstPlayer_init();
EXPORT_C_FUNC void GstPlayer_deinit(void *ptr);
EXPORT_C_FUNC void GstPlayer_setVideoPath(void *ptr, const char *path);
EXPORT_C_FUNC void GstPlayer_play(void *ptr);
EXPORT_C_FUNC void GstPlayer_pause(void *ptr);
EXPORT_C_FUNC void GstPlayer_stop(void *ptr);
// value range 0..1
EXPORT_C_FUNC void GstPlayer_seek(void *ptr, float normalized);
EXPORT_C_FUNC int GstPlayer_isPlaying(void *ptr);
// seconds if known, 0 if unknown (phase 1)
EXPORT_C_FUNC float GstPlayer_getDurationInSecs(void *ptr);
// callbacks
EXPORT_C_FUNC void
GstPlayer_onStateUpdate(void *ptr, void (*callback)(void *, const char *));
EXPORT_C_FUNC void GstPlayer_onProgress(void *ptr,
                                        void (*callback)(void *, float));
EXPORT_C_FUNC void GstPlayer_onError(void *ptr,
                                     void (*callback)(void *, const char *));
// placeholders
EXPORT_C_FUNC void GstPlayer_setSurfaceHandle(void *ptr, void *nativeSurface);
EXPORT_C_FUNC void GstPlayer_setMuteEmbeddedAudio(void *ptr, int mute);
// Video processing functions
EXPORT_C_FUNC void GstPlayer_setRotation(void *ptr, int degrees);
EXPORT_C_FUNC void GstPlayer_setVisualEffect(void *ptr, int effectId);
EXPORT_C_FUNC void GstPlayer_exportVideo(void *ptr, const char *outputPath,
                                         void (*completion)(const char *));

// MARK: UnifiedAVPlayer - Coordinated Audio/Video Player
EXPORT_C_FUNC void *UnifiedAVPlayer_getInstance();
EXPORT_C_FUNC void UnifiedAVPlayer_destroyInstance();
EXPORT_C_FUNC void UnifiedAVPlayer_dispose(void *ptr);

// Unified playback controls
EXPORT_C_FUNC void UnifiedAVPlayer_play(void *ptr);
EXPORT_C_FUNC void UnifiedAVPlayer_pause(void *ptr);
EXPORT_C_FUNC void UnifiedAVPlayer_stop(void *ptr);
EXPORT_C_FUNC void UnifiedAVPlayer_seek(void *ptr, float normalizedPos);
EXPORT_C_FUNC void UnifiedAVPlayer_togglePlayPause(void *ptr);

// Audio setup (required - primary media)
EXPORT_C_FUNC void UnifiedAVPlayer_setAudioData(void *ptr, const char *json);
EXPORT_C_FUNC void UnifiedAVPlayer_setAudioSettings(void *ptr,
                                                    const char *json);
EXPORT_C_FUNC void UnifiedAVPlayer_resetAudioPlayBuffer(void *ptr);
EXPORT_C_FUNC void
UnifiedAVPlayer_exportAudio(void *ptr, const char *outputPath,
                            void (*completion)(const char *));

// Video setup (optional - secondary media)
EXPORT_C_FUNC void UnifiedAVPlayer_setVideoPath(void *ptr, const char *path);
EXPORT_C_FUNC void UnifiedAVPlayer_setVideoSurfaceHandle(void *ptr,
                                                         void *handle);
EXPORT_C_FUNC void UnifiedAVPlayer_setVideoRotation(void *ptr, int degrees);
EXPORT_C_FUNC void UnifiedAVPlayer_setVideoVisualEffect(void *ptr,
                                                        int effectId);
EXPORT_C_FUNC void
UnifiedAVPlayer_exportVideo(void *ptr, const char *outputPath,
                            void (*completion)(const char *));

// State queries
EXPORT_C_FUNC float UnifiedAVPlayer_getDuration(void *ptr);
EXPORT_C_FUNC float UnifiedAVPlayer_getCurrentTime(void *ptr);
EXPORT_C_FUNC int UnifiedAVPlayer_isPlaying(void *ptr);
EXPORT_C_FUNC const char *UnifiedAVPlayer_getCurrentState(void *ptr);
EXPORT_C_FUNC int UnifiedAVPlayer_hasVideoLoaded(void *ptr);
EXPORT_C_FUNC void *UnifiedAVPlayer_getVideoPlayerPtr(void *ptr);

// Callbacks (unified - driven by audio timeline)
EXPORT_C_FUNC void UnifiedAVPlayer_onProgress(void *ptr,
                                              void (*callback)(void *, float));
EXPORT_C_FUNC void
UnifiedAVPlayer_onStateUpdate(void *ptr,
                              void (*callback)(void *, const char *));
EXPORT_C_FUNC void
UnifiedAVPlayer_onError(void *ptr, void (*callback)(void *, const char *));

// Recording support (audio only)
EXPORT_C_FUNC void UnifiedAVPlayer_prepareRecorder(void *ptr, const char *file);
EXPORT_C_FUNC void UnifiedAVPlayer_startRecorder(void *ptr);
EXPORT_C_FUNC void UnifiedAVPlayer_stopRecorder(void *ptr);
EXPORT_C_FUNC void UnifiedAVPlayer_onRecLevel(void *ptr,
                                              void (*callback)(void *, float));
EXPORT_C_FUNC void
UnifiedAVPlayer_onRecProgress(void *ptr, void (*callback)(void *, float));
EXPORT_C_FUNC void
UnifiedAVPlayer_onRecStateUpdate(void *ptr,
                                 void (*callback)(void *, const char *));
EXPORT_C_FUNC void
UnifiedAVPlayer_onRecError(void *ptr, void (*callback)(void *, const char *));

// Device management (audio only)
EXPORT_C_FUNC void UnifiedAVPlayer_setUpdatedDevices(void *ptr,
                                                     const char *json);
EXPORT_C_FUNC const char *UnifiedAVPlayer_getDeviceLatencyInfo(void *ptr);
EXPORT_C_FUNC void
UnifiedAVPlayer_onDeviceUpdate(void *ptr,
                               void (*callback)(void *, const char *));
