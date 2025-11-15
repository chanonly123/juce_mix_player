#include "GstPlayer.h"

GstPlayer::GstPlayer() {
    p = new GstVideoPlayerVars();
}

GstPlayer::~GstPlayer() {
    delete p;
}

bool GstPlayer::setURL(std::string url) {
    
    // stop any existing pipeline
    if (p->pipeline) {
        gst_element_set_state(p->pipeline, GST_STATE_NULL);
        gst_object_unref(p->pipeline);
        p->pipeline = nullptr;
        p->sink = nullptr;
    }
    
    // Build a simple pipeline: uridecodebin ! videoconvert ! glimagesink
    p->pipeline = gst_element_factory_make("playbin", "player");
    if (!p->pipeline) {
        return false;
    }
    
    char* uri = gst_filename_to_uri(url.c_str(), nullptr);
    g_object_set(G_OBJECT(p->pipeline), "uri", uri, nullptr);
    g_free(uri);
    
    // Prefer GL sink if available
    GstElement* videoSink = gst_element_factory_make("glimagesink", "videosink");
    if (!videoSink) {
        // fallback to autovideosink
        videoSink = gst_element_factory_make("autovideosink", "videosink");
    }
//    if (videoSink) {
//        g_object_set(G_OBJECT(p->pipeline), "video-sink", videoSink, nullptr);
//        p->sink = videoSink;
//    }
    
    g_object_set(G_OBJECT(p->pipeline), "video-sink", nullptr, nullptr);
    
    GstStateChangeReturn ret = gst_element_set_state(p->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        gst_object_unref(p->pipeline);
        p->pipeline = nullptr;
        p->sink = nullptr;
        return false;
    }
    
    return true;
}

void GstPlayer::setWindowHandle(void* nativeView) {
    if (!p->pipeline || !p->sink) return;
    // Use GstVideoOverlay API
    GstVideoOverlay* overlay = GST_VIDEO_OVERLAY(p->sink);
    if (!overlay) return;
    
    // On iOS/ObjC++ we pass UIView* or the view's layer pointer. The cast below
    // depends on how you want to pass it from Objective-C.
    // Use casting to uintptr_t / guintptr which is portable.
    gst_video_overlay_set_window_handle(overlay, (guintptr)nativeView);
}

void GstPlayer::play() {
    // Play implementation
}

void GstPlayer::pause() {
    // Pause implementation
}

void GstPlayer::stop() {
    if (p->pipeline) {
        gst_element_set_state(p->pipeline, GST_STATE_NULL);
        gst_object_unref(p->pipeline);
        p->pipeline = nullptr;
        p->sink = nullptr;
    }
}

void GstPlayer::dispose() {
    // Dispose implementation
}
