#ifdef __cplusplus
#define EXPORT_C_FUNC extern "C" __attribute__((visibility("default"))) __attribute__((used))
#else
#define EXPORT_C_FUNC extern
#endif

// iOS
EXPORT_C_FUNC void juceEnableLogs();
EXPORT_C_FUNC void juceMessageManagerInit();

EXPORT_C_FUNC void* JuceMixPlayer_init();
EXPORT_C_FUNC void JuceMixPlayer_deinit(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_play(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_pause(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_addItem(void* ptr, void *item);

EXPORT_C_FUNC void* JuceMixItem_init();
EXPORT_C_FUNC void JuceMixItem_deinit(void* ptr);
EXPORT_C_FUNC void JuceMixItem_setPath(void* ptr, const char* path, float begin, float end);

// android
#ifdef ANDROID
#include <jni.h>

EXPORT_C_FUNC void Java_com_rmsl_juce_Java_juceMessageManagerInit(JNIEnv* env, jclass);
#endif
