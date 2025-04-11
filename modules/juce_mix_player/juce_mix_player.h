#pragma once

/** BEGIN_JUCE_MODULE_DECLARATION

    ID:             juce_mix_player
    vendor:         chanonly123
    version:        0.0.1
    name:           Juce Mix Player
    description:    A player that plays audio coposition
    website:        https://github.com/chanonly123/juce_mix_player
    license:        MIT

    dependencies:       juce_audio_basics, juce_events
    OSXFrameworks:      CoreAudio CoreMIDI AudioToolbox
    iOSFrameworks:      CoreAudio CoreMIDI AudioToolbox AVFoundation

    END_JUCE_MODULE_DECLARATION
*/

#include "JuceMixPlayer.h"
#include "Models.h"
#include "Logger.h"
#include "TaskQueue.h"
