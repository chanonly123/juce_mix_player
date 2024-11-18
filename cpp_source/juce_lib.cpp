#include "includes/juce_lib.h"
#include "Logger.h"
#include "JuceMixPlayer.h"
#include "JuceMixItem.h"

void juceEnableLogs()
{
    enableLogsValue = true;
}

void juceMessageManagerInit()
{
    juce::MessageManager::getInstance();
}

// MARK: JuceMixPlayer

void *JuceMixPlayer_init()
{
    return new JuceMixPlayer();
}

void JuceMixPlayer_deinit(void *ptr)
{
    delete static_cast<JuceMixPlayer *>(ptr);
}

void JuceMixPlayer_play(void *ptr)
{
    static_cast<JuceMixPlayer *>(ptr)->play();
}

void JuceMixPlayer_pause(void *ptr)
{
    static_cast<JuceMixPlayer *>(ptr)->pause();
}

void JuceMixPlayer_addItem(void *ptr, void *item)
{
    static_cast<JuceMixPlayer *>(ptr)->addItem(static_cast<JuceMixItem *>(item));
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
