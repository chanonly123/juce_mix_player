#include <JuceHeader.h>
#include "includes/juce_lib.h"
#include "Logger.h"
#include "JuceMixPlayer.h"
#include "Models.h"

void Java_com_rmsl_juce_Java_juceMessageManagerInit()
{
    juce::MessageManager::getInstance();
}

void juceEnableLogs()
{
    enableLogsValue = true;
}

// MARK: JuceMixPlayer

void *JuceMixPlayer_init()
{
    return new JuceMixPlayerRef();
}

void JuceMixPlayer_deinit(void *ptr)
{
    juce::MessageManager::getInstanceWithoutCreating()->callAsync([=] {
        delete static_cast<JuceMixPlayerRef *>(ptr);
    });
}

void JuceMixPlayer_play(void *ptr)
{
    static_cast<JuceMixPlayerRef *>(ptr)->ptr->play();
}

void JuceMixPlayer_pause(void *ptr)
{
    static_cast<JuceMixPlayerRef *>(ptr)->ptr->pause();
}

void JuceMixPlayer_stop(void *ptr)
{
    static_cast<JuceMixPlayerRef *>(ptr)->ptr->stop();
}

void JuceMixPlayer_set(void* ptr, const char* json)
{
    static_cast<JuceMixPlayerRef *>(ptr)->ptr->set(json);
}

void JuceMixPlayer_onStateUpdate(void* ptr, void (*onStateUpdate)(const char*))
{
    static_cast<JuceMixPlayerRef *>(ptr)->ptr->onStateUpdate(onStateUpdate);
}

void JuceMixPlayer_onProgress(void* ptr, void (*onProgress)(float))
{
    static_cast<JuceMixPlayerRef *>(ptr)->ptr->onProgress(onProgress);
}

void JuceMixPlayer_onError(void* ptr, void (*onError)(const char*))
{
    static_cast<JuceMixPlayerRef *>(ptr)->ptr->onError(onError);
}
