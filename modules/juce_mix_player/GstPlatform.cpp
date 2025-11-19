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

bool GstPlatformIOS::isSupported() const {
    return true;
}

GstElement* GstPlatformIOS::createPipeline() {
    return gst_element_factory_make("playbin", "playbin");
}

GstElement* GstPlatformIOS::createVideoSink() {
    GstElement* sink = gst_element_factory_make("glimagesink", "videosink");
    if (sink) {
        g_object_set(sink, "force-aspect-ratio", TRUE, nullptr);
    }
    return sink;
}

void GstPlatformIOS::setupVideoProcessingBin(GstElement* videoBin, GstElement* videoFlip,
                                            GstElement* effectFilter, GstElement* videoSink) {
    // Create videoconvert for format negotiation
    GstElement* converter = gst_element_factory_make("videoconvert", "effect-converter");
    if (!converter) {
        PRINT("GstPlatformIOS: Failed to create videoconvert element");
        return;
    }

    // Add elements to bin
    gst_bin_add_many(GST_BIN(videoBin), videoFlip, converter, effectFilter, videoSink, nullptr);

    // Link elements: videoflip -> videoconvert -> effectfilter -> videosink
    if (!gst_element_link_many(videoFlip, converter, effectFilter, videoSink, nullptr)) {
        PRINT("GstPlatformIOS: Failed to link video processing elements");
        return;
    }

    // Create ghost pad for the bin
    GstPad* sinkPad = gst_element_get_static_pad(videoFlip, "sink");
    gst_element_add_pad(videoBin, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);
}

void GstPlatformIOS::applySurfaceHandle(GstElement* videoSink, void* surfaceHandle) {
    if (videoSink && surfaceHandle) {
        if (GST_IS_VIDEO_OVERLAY(videoSink)) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink), (guintptr) surfaceHandle);
        }
    }
}

GstElement* GstPlatformIOS::createEffectFilter(VisualEffect effect) {
    const char* filterName = "identity";
    switch (effect) {
        case VisualEffect::NONE:
            filterName = "identity";
            break;
        case VisualEffect::GRAINY:
        case VisualEffect::GRITTY:
        case VisualEffect::HYPER:
            filterName = "videobalance";
            break;
    }
    return gst_element_factory_make(filterName, "effectfilter");
}

void GstPlatformIOS::cleanup() {
    // iOS-specific cleanup if needed
}

void GstPlatformIOS::applyEffectParameters(GstElement* effectFilter, VisualEffect effect) {
    if (!effectFilter) return;

    // Only apply parameters if we have a videobalance element
    // Identity element doesn't have these properties
    if (effect == VisualEffect::NONE) {
        // Identity element - no parameters to set
        return;
    }

    // Check if this is actually a videobalance element
    GstElementFactory* factory = gst_element_get_factory(effectFilter);
    if (!factory) return;

    const gchar* factoryName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    if (g_strcmp0(factoryName, "videobalance") != 0) {
        // Not a videobalance element, don't try to set these properties
        return;
    }

    auto params = getEffectParameters(effect);
    for (const auto& param : params) {
        g_object_set(effectFilter, param.first.c_str(), std::stod(param.second), nullptr);
    }
}

std::map<std::string, std::string> GstPlatformIOS::getEffectParameters(VisualEffect effect) {
    std::map<std::string, std::string> params;

    switch (effect) {
        case VisualEffect::NONE:
            // Identity element - no parameters needed
            break;

        case VisualEffect::GRAINY:
            // Film-like grainy look
            params["brightness"] = "-0.05";
            params["contrast"]   = "0.85";
            params["saturation"] = "0.9";
            params["hue"]        = "0.0";
            break;

        case VisualEffect::GRITTY:
            // Harsh, dramatic gritty look
            params["brightness"] = "-0.1";
            params["contrast"]   = "1.35";
            params["saturation"] = "0.55";
            params["hue"]        = "0.0";
            break;

        case VisualEffect::HYPER:
            // Pop, vibrant, modern hyper color
            params["brightness"] = "0.05";
            params["contrast"]   = "1.2";
            params["saturation"] = "1.4";
            params["hue"]        = "0.02";
            break;
    }

    return params;
}
#endif

// Android Implementation (Placeholder)
#if JUCE_ANDROID
void GstPlatformAndroid::initialize() {
    // TODO: Implement gst_android_init() equivalent
    // This should initialize GStreamer for Android with proper plugin registration
    PRINT("GstPlatformAndroid: TODO - implement Android GStreamer initialization")
    gst_init(nullptr, nullptr);
}

bool GstPlatformAndroid::isSupported() const {
    // TODO: Check if GStreamer is properly initialized on Android
    return false; // Set to true once Android implementation is complete
}

GstElement* GstPlatformAndroid::createPipeline() {
    // TODO: Create Android-specific pipeline if needed, or use standard playbin
    return gst_element_factory_make("playbin", "playbin");
}

GstElement* GstPlatformAndroid::createVideoSink() {
    // TODO: Use Android-specific video sink
    // Options: androidvideosink, glimagesink, or autovideosink
    GstElement* sink = gst_element_factory_make("autovideosink", "videosink");
    if (sink) {
        g_object_set(sink, "force-aspect-ratio", TRUE, nullptr);
    }
    return sink;
}

void GstPlatformAndroid::setupVideoProcessingBin(GstElement* videoBin, GstElement* videoFlip,
                                                GstElement* effectFilter, GstElement* videoSink) {
    // TODO: Android-specific video processing bin setup
    // For now, use the same logic as iOS
    gst_bin_add_many(GST_BIN(videoBin), videoFlip, effectFilter, videoSink, nullptr);

    if (!gst_element_link_many(videoFlip, effectFilter, videoSink, nullptr)) {
        PRINT("GstPlatformAndroid: Failed to link video processing elements");
        return;
    }

    GstPad* sinkPad = gst_element_get_static_pad(videoFlip, "sink");
    gst_element_add_pad(videoBin, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);
}

void GstPlatformAndroid::applySurfaceHandle(GstElement* videoSink, void* surfaceHandle) {
    // TODO: Android-specific surface handling
    // This might involve JNI calls to set up the Android Surface
    if (videoSink && surfaceHandle) {
        if (GST_IS_VIDEO_OVERLAY(videoSink)) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink), (guintptr) surfaceHandle);
        }
    }
}

GstElement* GstPlatformAndroid::createEffectFilter(VisualEffect effect) {
    // TODO: Android-specific effect filters if needed
    // For now, use the same logic as iOS
    const char* filterName = "identity";
    switch (effect) {
        case VisualEffect::NONE:
            filterName = "identity";
            break;
        case VisualEffect::GRAINY:
        case VisualEffect::GRITTY:
        case VisualEffect::HYPER:
            filterName = "videobalance";
            break;
    }
    return gst_element_factory_make(filterName, "effectfilter");
}

void GstPlatformAndroid::applyEffectParameters(GstElement* effectFilter, VisualEffect effect) {
    // TODO: Android-specific effect parameter application
    // For now, use the same parameters as iOS
    if (!effectFilter) return;

    // Reuse iOS effect parameters for now
    std::map<std::string, std::string> params;
    switch (effect) {
        case VisualEffect::GRAINY:
            params["noise"] = "0.3";
            params["grain"] = "0.4";
            break;
        case VisualEffect::GRITTY:
            params["contrast"] = "1.5";
            params["saturation"] = "0.7";
            params["brightness"] = "-0.1";
            break;
        case VisualEffect::HYPER:
            params["contrast"] = "1.8";
            params["saturation"] = "1.6";
            params["brightness"] = "0.1";
            break;
        default:
            break;
    }

    for (const auto& param : params) {
        g_object_set(effectFilter, param.first.c_str(), std::stod(param.second), nullptr);
    }
}

void GstPlatformAndroid::cleanup() {
    // TODO: Android-specific cleanup
}
#endif
