#include "juce_mix_player.h"

#include "GstPlatform.cpp"
#include "GstPlayer.cpp"
#include "JuceMixPlayer.cpp"
#include "Logger.cpp"
#include "Models.cpp"
#include "TaskQueue.cpp"
#include "UnifiedAVPlayer.cpp"
#include "juce_wrapper_c.cpp"
extern "C" {
#include "gst_ios_init.cpp"
}