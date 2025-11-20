#include "includes/juce_wrapper_c.h"
#include "Logger.h"
#include "JuceMixPlayer.h"
// #include "GstPlayer.h"
#include "Models.h"
#include "GstPlayer.h"
// Global bridge for the single GstPlayer instance used by the Flutter video POC.
// This lets iOS (Swift) provide a UIView* as the window handle for the
// GStreamer video sink.
//static GstPlayer* g_globalGstPlayer = nullptr;
//static void* g_globalNativeView = nullptr;

void juce_init() {
    // JUCE message loop for the existing audio engine.
    juce::MessageManager::getInstance();
    // NOTE: GStreamer for video is now initialized lazily by GstPlayer
    // on iOS via gst_ios_init(), so we don't block app startup here.
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

void JuceMixPlayer_export(void* ptr,
                          const char *outputPath,
                          void (*completion)(const char*)) {
    return static_cast<JuceMixPlayer *>(ptr)->exportToFile(outputPath, completion);
}

// Utility methods
int JuceMixPlayer_fileExists(const char* filePath) {
    juce::File file(filePath);
    return file.exists() ? 1 : 0;
}


// MARK: GstPlayer (Phase 1 skeleton)
void* GstPlayer_init() {
    return new GstPlayer();
}

void GstPlayer_deinit(void* ptr) {
    delete static_cast<GstPlayer*>(ptr);
}

void GstPlayer_setVideoPath(void* ptr, const char* path) {
    static_cast<GstPlayer*>(ptr)->setVideoPath(path);
}

void GstPlayer_play(void* ptr) {
    static_cast<GstPlayer*>(ptr)->play();
}

void GstPlayer_pause(void* ptr) {
    static_cast<GstPlayer*>(ptr)->pause();
}

void GstPlayer_stop(void* ptr) {
    static_cast<GstPlayer*>(ptr)->stop();
}

void GstPlayer_seek(void* ptr, float normalized) {
    static_cast<GstPlayer*>(ptr)->seek(normalized);
}

int GstPlayer_isPlaying(void* ptr) {
    return static_cast<GstPlayer*>(ptr)->isPlaying();
}

float GstPlayer_getDurationInSecs(void* ptr) {
    return static_cast<GstPlayer*>(ptr)->getDurationInSecs();
}

void GstPlayer_onStateUpdate(void* ptr, void (*callback)(void*, const char*)) {
    static_cast<GstPlayer*>(ptr)->onStateUpdateCallback = callback;
}

void GstPlayer_onProgress(void* ptr, void (*callback)(void*, float)) {
    static_cast<GstPlayer*>(ptr)->onProgressCallback = callback;
}

void GstPlayer_onError(void* ptr, void (*callback)(void*, const char*)) {
    static_cast<GstPlayer*>(ptr)->onErrorCallback = callback;
}

void GstPlayer_setSurfaceHandle(void* ptr, void* nativeSurface) {
    static_cast<GstPlayer*>(ptr)->setSurfaceHandle(nativeSurface);
}

void GstPlayer_setMuteEmbeddedAudio(void* ptr, int mute) {
    static_cast<GstPlayer*>(ptr)->setMuteEmbeddedAudio(mute);
}

void GstPlayer_setRotation(void* ptr, int degrees) {
    static_cast<GstPlayer*>(ptr)->setRotation(degrees);
}

void GstPlayer_setVisualEffect(void* ptr, int effectId) {
    static_cast<GstPlayer*>(ptr)->setVisualEffect(effectId);
}

void GstPlayer_exportVideo(void* ptr, const char* outputPath, void (*completion)(const char*)) {
    static_cast<GstPlayer*>(ptr)->exportVideo(outputPath, completion);
}




// GstPlayer

//void* GstPlayer_init() {
//    auto* player = new GstPlayer();
//    // Register as the global video player instance so that if a native view
//    // was already provided from iOS we can immediately attach to it.
//    g_globalGstPlayer = player;
//    if (g_globalNativeView != nullptr) {
//        player->setWindowHandle(g_globalNativeView);
//    }
//    return player;
//}
//
//void GstPlayer_setWindowHandleGlobal(void* nativeView) {
//    g_globalNativeView = nativeView;
//    if (g_globalGstPlayer != nullptr) {
//        g_globalGstPlayer->setWindowHandle(nativeView);
//    }
//}
//
//void GstPlayer_setWindowHandle(void* ptr, void* nativeView) {
//    static_cast<GstPlayer *>(ptr)->setWindowHandle(nativeView);
//}
//
//int GstPlayer_setURL(void* ptr, const char* url) {
//    return static_cast<GstPlayer *>(ptr)->setURL(url) ? 1 : 0;
//}
//
//void GstPlayer_dispose(void* ptr) {
//    static_cast<GstPlayer *>(ptr)->dispose();
//}
//
//void GstPlayer_play(void* ptr) {
//    static_cast<GstPlayer *>(ptr)->play();
//}
//
//void GstPlayer_pause(void* ptr) {
//    static_cast<GstPlayer *>(ptr)->pause();
//}
//
//void GstPlayer_stop(void* ptr) {
//    static_cast<GstPlayer *>(ptr)->stop();
//}
//
//void GstPlayer_seek(void* ptr, float position) {
//    static_cast<GstPlayer *>(ptr)->seek(position);
//}
