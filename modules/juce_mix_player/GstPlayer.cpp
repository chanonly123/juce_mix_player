#include "GstPlayer.h"
#include <iostream>

// Bus callback for handling GStreamer messages
static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    GstPlayer* player = static_cast<GstPlayer*>(data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(msg, &err, &debug);
            std::cerr << "GStreamer Error: " << err->message << std::endl;
            if (debug) {
                std::cerr << "Debug info: " << debug << std::endl;
            }
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "GStreamer: End of stream" << std::endl;
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(player->pipeline)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                std::cout << "GStreamer: State changed from "
                         << gst_element_state_get_name(old_state) << " to "
                         << gst_element_state_get_name(new_state) << std::endl;
            }
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError *err;
            gchar *debug;
            gst_message_parse_warning(msg, &err, &debug);
            std::cerr << "GStreamer Warning: " << err->message << std::endl;
            if (debug) {
                std::cerr << "Debug info: " << debug << std::endl;
            }
            g_error_free(err);
            g_free(debug);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

GstPlayer::GstPlayer() {
    p = new GstVideoPlayerVars();
}

GstPlayer::~GstPlayer() {
    stop();
    delete p;
}

bool GstPlayer::setURL(std::string url) {
    std::lock_guard<std::mutex> lock(p->mtx);

    // stop any existing pipeline
    if (p->pipeline) {
        gst_element_set_state(p->pipeline, GST_STATE_NULL);
        gst_object_unref(p->pipeline);
        p->pipeline = nullptr;
        p->sink = nullptr;
        pipeline = nullptr;
        sink = nullptr;
    }

    std::cout << "GstPlayer: Creating VIDEO pipeline for URL: " << url << std::endl;

    // Build a simple pipeline using playbin
    p->pipeline = gst_element_factory_make("playbin", "video_player");
    if (!p->pipeline) {
        std::cerr << "GstPlayer: Failed to create playbin element" << std::endl;
        return false;
    }

    // For AUDIO ONLY
    //  p->pipeline = gst_element_factory_make("playbin", "audio_player");
    // if (!p->pipeline) {
    //     std::cerr << "GstAudioPlayer: Failed to create playbin element" << std::endl;
    //     return false;
    // }

    // Keep a direct pointer for bus callback comparisons
    pipeline = p->pipeline;

    // Convert file path to URI
    char* uri = gst_filename_to_uri(url.c_str(), nullptr);
    if (!uri) {
        std::cerr << "GstPlayer: Failed to convert path to URI: " << url << std::endl;
        gst_object_unref(p->pipeline);
        p->pipeline = nullptr;
        pipeline = nullptr;
        return false;
    }

    std::cout << "GstPlayer: Setting URI: " << uri << std::endl;
    g_object_set(G_OBJECT(p->pipeline), "uri", uri, nullptr);
    g_free(uri);

    // Configure video sink for iOS. We prefer glimagesink so we can use GstVideoOverlay
    // against a UIView* passed from Flutter (via Swift).
    GstElement* videoSink = gst_element_factory_make("glimagesink", "videosink");
    if (videoSink) {
        std::cout << "GstPlayer: Using glimagesink for video output" << std::endl;
        g_object_set(G_OBJECT(p->pipeline), "video-sink", videoSink, nullptr);
        p->sink = videoSink;
        sink = videoSink;

        // If a native window handle was already provided (from iOS), attach it now.
        if (p->windowHandle && GST_IS_VIDEO_OVERLAY(videoSink)) {
            GstVideoOverlay* overlay = GST_VIDEO_OVERLAY(videoSink);
            gst_video_overlay_set_window_handle(overlay, (guintptr)p->windowHandle);
            std::cout << "GstPlayer: Applied stored window handle to video sink" << std::endl;
        }
    } else {
        std::cerr << "GstPlayer: Warning - glimagesink not available, using default video sink" << std::endl;
        p->sink = nullptr;
        sink = nullptr;
    }

    // // Configure audio sink for iOS
    // GstElement* audioSink = gst_element_factory_make("osxaudiosink", "audiosink");
    // if (audioSink) {
    //     std::cout << "GstAudioPlayer: Using osxaudiosink for audio output" << std::endl;
    //     g_object_set(G_OBJECT(p->pipeline), "audio-sink", audioSink, nullptr);
    // } else {
    //     std::cerr << "GstAudioPlayer: Warning - osxaudiosink not available, using default" << std::endl;
    // }

    // Mute audio by default for this video-focused player so that you can
    // easily remove or re-enable embedded audio later without affecting video.
//    GParamSpec* muteProp = g_object_class_find_property(G_OBJECT_GET_CLASS(p->pipeline), "mute");
//    if (muteProp) {
//        g_object_set(G_OBJECT(p->pipeline), "mute", TRUE, nullptr);
//        std::cout << "GstPlayer: Audio muted by default" << std::endl;
//    }

    // Set up bus to watch for messages
    GstBus* bus = gst_element_get_bus(p->pipeline);
    gst_bus_add_watch(bus, bus_callback, this);
    gst_object_unref(bus);

    // Set pipeline to PAUSED state first (preroll)
    std::cout << "GstPlayer: Setting pipeline to PAUSED state" << std::endl;
    GstStateChangeReturn ret = gst_element_set_state(p->pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "GstPlayer: Failed to set pipeline to PAUSED state" << std::endl;
        gst_object_unref(p->pipeline);
        p->pipeline = nullptr;
        p->sink = nullptr;
        pipeline = nullptr;
        sink = nullptr;
        return false;
    }

    std::cout << "GstPlayer: VIDEO pipeline created successfully, ready to play" << std::endl;
    return true;
}

void GstPlayer::setWindowHandle(void* nativeView) {
    std::lock_guard<std::mutex> lock(p->mtx);

    // Always remember the last window handle so that we can apply it when the
    // pipeline/sink become available (or are recreated).
    p->windowHandle = nativeView;

    if (!p->pipeline || !p->sink) {
        std::cout << "GstPlayer: Stored window handle, pipeline/sink not ready yet" << std::endl;
        return;
    }

    // Use GstVideoOverlay API for video rendering
    if (GST_IS_VIDEO_OVERLAY(p->sink)) {
        GstVideoOverlay* overlay = GST_VIDEO_OVERLAY(p->sink);
        // On iOS/ObjC++ we pass UIView* or the view's layer pointer
        gst_video_overlay_set_window_handle(overlay, (guintptr)nativeView);
        std::cout << "GstPlayer: Video overlay window handle set" << std::endl;
    } else {
        std::cout << "GstPlayer: Sink does not implement GstVideoOverlay" << std::endl;
    }
}

void GstPlayer::play() {
    std::lock_guard<std::mutex> lock(p->mtx);
    if (!p->pipeline) {
        std::cerr << "GstPlayer: Cannot play - no pipeline" << std::endl;
        return;
    }

    std::cout << "GstPlayer: Setting pipeline to PLAYING state" << std::endl;
    GstStateChangeReturn ret = gst_element_set_state(p->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "GstPlayer: Failed to set pipeline to PLAYING state" << std::endl;
    }
}

void GstPlayer::pause() {
    std::lock_guard<std::mutex> lock(p->mtx);
    if (!p->pipeline) {
        std::cerr << "GstPlayer: Cannot pause - no pipeline" << std::endl;
        return;
    }

    std::cout << "GstPlayer: Setting pipeline to PAUSED state" << std::endl;
    GstStateChangeReturn ret = gst_element_set_state(p->pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "GstPlayer: Failed to set pipeline to PAUSED state" << std::endl;
    }
}

void GstPlayer::stop() {
    std::lock_guard<std::mutex> lock(p->mtx);
    if (p->pipeline) {
        std::cout << "GstPlayer: Stopping pipeline" << std::endl;
        gst_element_set_state(p->pipeline, GST_STATE_NULL);
        gst_object_unref(p->pipeline);
        p->pipeline = nullptr;
        p->sink = nullptr;
        pipeline = nullptr;
        sink = nullptr;
    }
}

void GstPlayer::seek(double position) {
    std::lock_guard<std::mutex> lock(p->mtx);
    if (!p->pipeline) {
        std::cerr << "GstPlayer: Cannot seek - no pipeline" << std::endl;
        return;
    }

    // Query duration
    gint64 duration;
    if (!gst_element_query_duration(p->pipeline, GST_FORMAT_TIME, &duration)) {
        std::cerr << "GstPlayer: Failed to query duration for seek" << std::endl;
        return;
    }

    // Calculate target position (position is 0.0 to 1.0)
    gint64 seekPos = (gint64)(position * duration);
    
    std::cout << "GstPlayer: Seeking to position " << position 
              << " (time: " << (seekPos / GST_SECOND) << "s)" << std::endl;

    // Perform seek
    if (!gst_element_seek_simple(p->pipeline, GST_FORMAT_TIME,
                                  (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                  seekPos)) {
        std::cerr << "GstPlayer: Seek failed" << std::endl;
    } else {
        std::cout << "GstPlayer: Seek successful" << std::endl;
    }
}

void GstPlayer::dispose() {
    std::cout << "GstPlayer: Disposing player" << std::endl;
    stop();
}
