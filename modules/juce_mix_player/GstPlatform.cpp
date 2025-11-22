#include "GstPlatform.h"
#include "GstPlayer.h" // For enums
#include "Logger.h"

#if JUCE_IOS
#include "gst_ios_init.h"
#include <mutex>
static std::once_flag gGstInitOnce;
#endif

// Factory method implementation
std::unique_ptr<GstPlatform> GstPlatform::create() {
#if JUCE_IOS
    return std::make_unique<GstPlatformIOS>();
#elif JUCE_ANDROID
    return std::make_unique<GstPlatformAndroid>();
#endif
}

// iOS Implementation
#if JUCE_IOS
void GstPlatformIOS::initialize() {
    std::call_once(gGstInitOnce, [] {
        PRINT("GstPlatformIOS: calling gst_ios_init()")
        gst_ios_init();
    });
}
#endif

// Android Implementation (Placeholder)
#if JUCE_ANDROID
void GstPlatformAndroid::initialize() {
    PRINT("GstPlatformAndroid: TODO - implement Android GStreamer initialization")
    gst_init(nullptr, nullptr);
}
#endif
