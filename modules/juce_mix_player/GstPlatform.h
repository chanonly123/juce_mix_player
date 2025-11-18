#pragma once

#include <JuceHeader.h>
#include <memory>
#include <string>
#include <map>

extern "C" {
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
}

// Forward declarations
enum class VideoRotation;
enum class VisualEffect;

/**
 * Abstract base class for platform-specific GStreamer implementations.
 * This separates platform-specific code from common video playback logic.
 */
class GstPlatform {
public:
    virtual ~GstPlatform() = default;
    
    // Platform initialization
    virtual void initialize() = 0;
    virtual bool isSupported() const = 0;
    
    // Pipeline management
    virtual GstElement* createPipeline() = 0;
    virtual GstElement* createVideoSink() = 0;
    virtual void setupVideoProcessingBin(GstElement* videoBin, GstElement* videoFlip, 
                                       GstElement* effectFilter, GstElement* videoSink) = 0;
    
    // Surface/window handling
    virtual void applySurfaceHandle(GstElement* videoSink, void* surfaceHandle) = 0;
    
    // Effect management
    virtual GstElement* createEffectFilter(VisualEffect effect) = 0;
    virtual void applyEffectParameters(GstElement* effectFilter, VisualEffect effect) = 0;
    
    // Platform-specific cleanup
    virtual void cleanup() = 0;
    
    // Factory method
    static std::unique_ptr<GstPlatform> create();
};

/**
 * iOS-specific GStreamer implementation
 */
class GstPlatformIOS : public GstPlatform {
public:
    void initialize() override;
    bool isSupported() const override;
    
    GstElement* createPipeline() override;
    GstElement* createVideoSink() override;
    void setupVideoProcessingBin(GstElement* videoBin, GstElement* videoFlip, 
                               GstElement* effectFilter, GstElement* videoSink) override;
    
    void applySurfaceHandle(GstElement* videoSink, void* surfaceHandle) override;
    
    GstElement* createEffectFilter(VisualEffect effect) override;
    void applyEffectParameters(GstElement* effectFilter, VisualEffect effect) override;
    
    void cleanup() override;

private:
    std::map<std::string, std::string> getEffectParameters(VisualEffect effect);
};

/**
 * Android-specific GStreamer implementation (placeholder)
 */
class GstPlatformAndroid : public GstPlatform {
public:
    void initialize() override;
    bool isSupported() const override;
    
    GstElement* createPipeline() override;
    GstElement* createVideoSink() override;
    void setupVideoProcessingBin(GstElement* videoBin, GstElement* videoFlip, 
                               GstElement* effectFilter, GstElement* videoSink) override;
    
    void applySurfaceHandle(GstElement* videoSink, void* surfaceHandle) override;
    
    GstElement* createEffectFilter(VisualEffect effect) override;
    void applyEffectParameters(GstElement* effectFilter, VisualEffect effect) override;
    
    void cleanup() override;
};

/**
 * Fallback implementation for unsupported platforms
 */
class GstPlatformFallback : public GstPlatform {
public:
    void initialize() override {}
    bool isSupported() const override { return false; }
    
    GstElement* createPipeline() override { return nullptr; }
    GstElement* createVideoSink() override { return nullptr; }
    void setupVideoProcessingBin(GstElement*, GstElement*, GstElement*, GstElement*) override {}
    
    void applySurfaceHandle(GstElement*, void*) override {}
    
    GstElement* createEffectFilter(VisualEffect) override { return nullptr; }
    void applyEffectParameters(GstElement*, VisualEffect) override {}
    
    void cleanup() override {}
};
