#include <JuceHeader.h>
#include "includes/juce_lib.h"
#include "Logger.h"
#include "JuceMixPlayer.h"
#include "Models.h"

void Java_com_rmsl_juce_Java_juceMessageManagerInit()
{
    juce::MessageManager::getInstance();
}

// public method to enable/disable logging
void juce_enableLogs(int enable)
{
    enableLogsValue = enable == 1;
}

// MARK: JuceMixPlayer

void *JuceMixPlayer_init(int record, int play)
{
    return new JuceMixPlayer(record, play);
}

void JuceMixPlayer_deinit(void *ptr)
{
    static_cast<JuceMixPlayer *>(ptr)->dispose();
}

void JuceMixPlayer_play(void *ptr)
{
    static_cast<JuceMixPlayer *>(ptr)->play();
}

void JuceMixPlayer_pause(void *ptr)
{
    static_cast<JuceMixPlayer *>(ptr)->pause();
}

void JuceMixPlayer_stop(void *ptr)
{
    static_cast<JuceMixPlayer *>(ptr)->stop();
}

void JuceMixPlayer_set(void* ptr, const char* json)
{
    static_cast<JuceMixPlayer *>(ptr)->setJson(json);
}

void JuceMixPlayer_onStateUpdate(void* ptr, void (*onStateUpdate)(void* ptr, const char*))
{
    static_cast<JuceMixPlayer *>(ptr)->onStateUpdate(onStateUpdate);
}

void JuceMixPlayer_onProgress(void* ptr, void (*onProgress)(void* ptr, float))
{
    static_cast<JuceMixPlayer *>(ptr)->onProgress(onProgress);
}

void JuceMixPlayer_onError(void* ptr, void (*onError)(void* ptr, const char*))
{
    static_cast<JuceMixPlayer *>(ptr)->onError(onError);
}

float JuceMixPlayer_getDuration(void *ptr)
{
    return static_cast<JuceMixPlayer *>(ptr)->getDuration();
}

int JuceMixPlayer_isPlaying(void *ptr)
{
    return static_cast<JuceMixPlayer *>(ptr)->isPlaying();
}

void JuceMixPlayer_seek(void* ptr, float value)
{
    static_cast<JuceMixPlayer *>(ptr)->seek(value);
}

void JuceMixPlayer_startRecorder(void* ptr, const char* file)
{
    static_cast<JuceMixPlayer *>(ptr)->startRecorder(file);

}

void JuceMixPlayer_stopRecorder(void* ptr)
{
    static_cast<JuceMixPlayer *>(ptr)->stopRecorder();
}
