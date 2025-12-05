#pragma once

#include <JuceHeader.h>
#include <map>
#include <memory>
#include <string>

extern "C" {
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
}

// Forward declarations
enum class VisualEffect;

/**
 * Abstract base class for platform-specific GStreamer implementations.
 * This separates platform-specific code from common video playback logic.
 */
class GstPlatform {
public:
    virtual ~GstPlatform() = default;
    virtual void initialize() = 0;
    static std::unique_ptr<GstPlatform> create();
};

/**
 * iOS-specific GStreamer implementation
 */
class GstPlatformIOS : public GstPlatform {
public:
    void initialize() override;
};

/**
 * Android-specific GStreamer implementation (placeholder)
 */
class GstPlatformAndroid : public GstPlatform {
public:
    void initialize() override;
};
