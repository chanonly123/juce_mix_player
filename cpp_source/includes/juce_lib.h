#ifdef __cplusplus
#define EXPORT_C_FUNC extern "C" __attribute__((visibility("default"))) __attribute__((used))
#else
#define EXPORT_C_FUNC extern
#endif

EXPORT_C_FUNC void Java_com_rmsl_juce_Java_juceMessageManagerInit();
EXPORT_C_FUNC void juceEnableLogs();

EXPORT_C_FUNC void* JuceMixPlayer_init();
EXPORT_C_FUNC void JuceMixPlayer_deinit(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_play(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_pause(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_set(void* ptr, const char* json);
EXPORT_C_FUNC void JuceMixPlayer_onStateUpdate(void* ptr, void (*JuceMixPlayerOnStateUpdate)(const char*));
EXPORT_C_FUNC void JuceMixPlayer_onProgress(void* ptr, void (*onProgress)(float));
