#include "includes/juce_wrapper_c.h"
#include "Logger.h"
#include "JuceMixPlayer.h"
#include "Models.h"

void juce_init() {
    juce::MessageManager::getInstance();
}

void Java_com_rmsl_juce_Native_juceMessageManagerInit() {
    juce_init();
}

// public method to enable/disable logging
void juce_enableLogs(int enable) {
    enableLogsValue = enable == 1;
}

// MARK: JuceMixPlayer

void *JuceMixPlayer_init() {
    return new JuceMixPlayer();
}

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

void JuceMixPlayer_set(void* ptr, const char* json) {
    static_cast<JuceMixPlayer *>(ptr)->setJson(json);
}

void JuceMixPlayer_setSettings(void* ptr, const char* json) {
    static_cast<JuceMixPlayer *>(ptr)->setSettings(json);
}

void JuceMixPlayer_onStateUpdate(void* ptr, void (*onStateUpdate)(void* ptr, const char*)) {
    static_cast<JuceMixPlayer *>(ptr)->onStateUpdateCallback = onStateUpdate;
}

void JuceMixPlayer_onProgress(void* ptr, void (*onProgress)(void* ptr, float)) {
    static_cast<JuceMixPlayer *>(ptr)->onProgressCallback = onProgress;
}

void JuceMixPlayer_onError(void* ptr, void (*onError)(void* ptr, const char*)) {
    static_cast<JuceMixPlayer *>(ptr)->onErrorCallback = onError;
}

float JuceMixPlayer_getDuration(void *ptr) {
    return static_cast<JuceMixPlayer *>(ptr)->getDuration();
}

int JuceMixPlayer_isPlaying(void *ptr) {
    return static_cast<JuceMixPlayer *>(ptr)->isPlaying();
}

void JuceMixPlayer_seek(void* ptr, float value) {
    static_cast<JuceMixPlayer *>(ptr)->seek(value);
}

void JuceMixPlayer_prepareRecorder(void* ptr, const char* file) {
    static_cast<JuceMixPlayer *>(ptr)->prepareRecorder(file);
}

void JuceMixPlayer_startRecorder(void* ptr) {
    static_cast<JuceMixPlayer *>(ptr)->startRecorder();
}

void JuceMixPlayer_stopRecorder(void* ptr) {
    static_cast<JuceMixPlayer *>(ptr)->stopRecorder();
}

void JuceMixPlayer_onRecStateUpdate(void* ptr, void (*onStateUpdate)(void* ptr, const char*)) {
    static_cast<JuceMixPlayer *>(ptr)->onRecStateUpdateCallback = onStateUpdate;
}

void JuceMixPlayer_onRecProgress(void* ptr, void (*onProgress)(void* ptr, float)) {
    static_cast<JuceMixPlayer *>(ptr)->onRecProgressCallback = onProgress;
}

void JuceMixPlayer_onRecError(void* ptr, void (*onError)(void* ptr, const char*)) {
    static_cast<JuceMixPlayer *>(ptr)->onRecErrorCallback = onError;
}

void JuceMixPlayer_onRecLevel(void* ptr, void (*onLevel)(void* ptr, float)) {
    static_cast<JuceMixPlayer *>(ptr)->onRecLevelCallback = onLevel;
}

void JuceMixPlayer_onDeviceUpdate(void* ptr, void (*onDeviceUpdate)(void* ptr, const char*)) {
    static_cast<JuceMixPlayer *>(ptr)->onDeviceUpdateCallback = onDeviceUpdate;
}

void JuceMixPlayer_setUpdatedDevices(void* ptr, const char* json) {
    static_cast<JuceMixPlayer *>(ptr)->setUpdatedDevices(json);
}

const char* JuceMixPlayer_getDeviceLatencyInfo(void *ptr) {
    return static_cast<JuceMixPlayer *>(ptr)->getDeviceLatencyInfo();
}
