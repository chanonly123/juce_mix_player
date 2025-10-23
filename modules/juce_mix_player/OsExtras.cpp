#include "OsExtras.h"

#if JUCE_IOS

#import <AVFoundation/AVFoundation.h>

// MARK: set audio session for iOS
bool setAudioSessionPlay() {
    NSUInteger options = AVAudioSessionCategoryOptionMixWithOthers;
    NSError* error = nil;
    [[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryPlayback
                                     withOptions: options
                                           error: &error];
    return error == nil;
}

bool setAudioSessionRecord(MixerSettings& settings) {
    NSUInteger options = AVAudioSessionCategoryOptionDefaultToSpeaker
    | AVAudioSessionCategoryOptionAllowBluetoothA2DP
    | AVAudioSessionCategoryOptionAllowBluetoothHFP;
    
    if (settings.dissallowBluetoothMic) {
        options = options & (~AVAudioSessionCategoryOptionAllowBluetoothHFP);
    }
    
    NSError* error = nil;
    [[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryPlayAndRecord
                                     withOptions: options
                                           error: &error];
    return error == nil;
}
#elif JUCE_MAC
bool setAudioSessionPlay() { return true; }
bool setAudioSessionRecord(MixerSettings& settings) { return true; }
#else
bool setAudioSessionPlay() { return true; }
bool setAudioSessionRecord(MixerSettings& settings) { return true; }
#endif

juce::AudioDeviceManager* sharedDeviceManager()
{
    if (_sharedDeviceManager == nullptr) {
        _sharedDeviceManager = new juce::AudioDeviceManager();
    }
    return _sharedDeviceManager;
}
