#include "includes/juce_lib.h"
#include "Logger.h"
#include "JuceMixPlayer.h"
#include "JuceMixItem.h"
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

void JuceMixPlayer_reset(void* ptr, const char* json)
{
    juce::MessageManager::getInstance()->callAsync([=] {
        static_cast<JuceMixPlayerRef *>(ptr)->ptr->reset(json);
    });
}

// MARK: JuceMixItem

void *JuceMixItem_init()
{
    return new JuceMixItem();
}

void JuceMixItem_deinit(void *ptr)
{
    delete static_cast<JuceMixItem *>(ptr);
}

void JuceMixItem_setPath(void *ptr, const char *path, float begin, float end)
{
    static_cast<JuceMixItem *>(ptr)->setPath(juce::String(path), begin, end);
}

void testParse(const char* json) {
    MixerModel::parse(json);
}
