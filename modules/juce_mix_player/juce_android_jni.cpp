#if JUCE_ANDROID

#include "includes/juce_wrapper_c.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_flutter_1app_GstVideoPlayerView_nativeSetSurfaceHandle(
    JNIEnv *env, jobject thiz, jlong playerPtr, jobject surface) {

  if (playerPtr == 0) {
    return;
  }

  void *ptr = reinterpret_cast<void *>(playerPtr);

  if (surface == nullptr || env->IsSameObject(surface, nullptr)) {
    GstPlayer_setSurfaceHandle(ptr, nullptr);
  } else {
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    GstPlayer_setSurfaceHandle(ptr, nativeWindow);
  }
}

JNIEXPORT void JNICALL
Java_com_example_flutter_1app_UnifiedVideoView_nativeSetVideoSurfaceHandle(
    JNIEnv *env, jobject thiz, jlong playerPtr, jobject surface) {

  if (playerPtr == 0) {
    return;
  }

  void *ptr = reinterpret_cast<void *>(playerPtr);

  if (surface == nullptr || env->IsSameObject(surface, nullptr)) {
    UnifiedAVPlayer_setVideoSurfaceHandle(ptr, nullptr);
  } else {
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    UnifiedAVPlayer_setVideoSurfaceHandle(ptr, nativeWindow);
  }
}
}

#endif
