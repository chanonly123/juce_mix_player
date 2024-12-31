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
    std::promise<void*> promise;
    juce::MessageManager::getInstance()->callAsync([&promise] {
        promise.set_value(new JuceMixPlayerRef());
    });
    return promise.get_future().get();
}

void JuceMixPlayer_deinit(void *ptr)
{
    juce::MessageManager::getInstance()->callAsync([=] {
        delete static_cast<JuceMixPlayerRef *>(ptr);
    });
}

void JuceMixPlayer_play(void *ptr)
{
    juce::MessageManager::getInstance()->callAsync([=] {
        static_cast<JuceMixPlayerRef *>(ptr)->ptr->play();
    });
}

void JuceMixPlayer_pause(void *ptr)
{
    juce::MessageManager::getInstance()->callAsync([=] {
        static_cast<JuceMixPlayerRef *>(ptr)->ptr->pause();
    });
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
