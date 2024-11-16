



#define EXPORT_C_FUNC extern

EXPORT_C_FUNC void juceEnableLogs();

EXPORT_C_FUNC void* JuceMixPlayer_init();
EXPORT_C_FUNC void JuceMixPlayer_deinit(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_play(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_pause(void* ptr);
EXPORT_C_FUNC void JuceMixPlayer_addItem(void* ptr, void* item);

EXPORT_C_FUNC void* JuceMixItem_init();
EXPORT_C_FUNC void JuceMixItem_deinit(void* ptr);
EXPORT_C_FUNC void JuceMixItem_setPath(void* ptr, const char* path, float begin, float end);

