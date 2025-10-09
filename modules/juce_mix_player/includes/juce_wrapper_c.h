#pragma once

#ifdef __cplusplus
#define EXPORT_C_FUNC extern "C" __attribute__((visibility("default"))) __attribute__((used))
#else
#define EXPORT_C_FUNC extern
#endif

EXPORT_C_FUNC void juce_init();
EXPORT_C_FUNC void Java_com_rmsl_juce_Native_juceMessageManagerInit();
EXPORT_C_FUNC void juce_enableLogs(int enable);

EXPORT_C_FUNC void* JuceMixPlayer_init();
EXPORT_C_FUNC void JuceMixPlayer_deinit(void* ptr);

EXPORT_C_FUNC void JuceMixPlayer_play(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_pause(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_stop(void *ptr);

EXPORT_C_FUNC void JuceMixPlayer_set(void* ptr, const char* json);

EXPORT_C_FUNC void JuceMixPlayer_setSettings(void* ptr, const char* json);

EXPORT_C_FUNC void JuceMixPlayer_onStateUpdate(void* ptr, void (*JuceMixPlayerCallbackString)(void*, const char*));

/// callback with progress value range 0 to 1
EXPORT_C_FUNC void JuceMixPlayer_onProgress(void* ptr, void (*onProgress)(void*, float));

EXPORT_C_FUNC void JuceMixPlayer_onError(void* ptr, void (*onError)(void*, const char*));

/// value returns time in seconds
EXPORT_C_FUNC float JuceMixPlayer_getDuration(void *ptr);

/// returns 1 if playing else 0
EXPORT_C_FUNC int JuceMixPlayer_isPlaying(void *ptr);

/// value range 0 to 1
EXPORT_C_FUNC void JuceMixPlayer_seek(void* ptr, float value);

// MARK: Recorder

EXPORT_C_FUNC void JuceMixPlayer_prepareRecorder(void* ptr, const char* file);

EXPORT_C_FUNC void JuceMixPlayer_startRecorder(void* ptr);

EXPORT_C_FUNC void JuceMixPlayer_stopRecorder(void* ptr);

EXPORT_C_FUNC void JuceMixPlayer_onRecStateUpdate(void* ptr, void (*onStateUpdate)(void* ptr, const char*));

EXPORT_C_FUNC void JuceMixPlayer_onRecProgress(void* ptr, void (*onProgress)(void* ptr, float));

EXPORT_C_FUNC void JuceMixPlayer_onRecError(void* ptr, void (*onError)(void* ptr, const char*));

EXPORT_C_FUNC void JuceMixPlayer_onRecLevel(void* ptr, void (*onLevel)(void* ptr, float));

EXPORT_C_FUNC void JuceMixPlayer_onDeviceUpdate(void* ptr, void (*onDeviceUpdate)(void* ptr, const char*));

EXPORT_C_FUNC void JuceMixPlayer_setUpdatedDevices(void* ptr, const char* json);

EXPORT_C_FUNC const char* JuceMixPlayer_getDeviceLatencyInfo(void *ptr);

EXPORT_C_FUNC void JuceMixPlayer_export(void* ptr,
                                        const char *outputPath,
                                        void (*completion)(const char*));

EXPORT_C_FUNC int JuceMixPlayer_fileExists(const char* filePath);
