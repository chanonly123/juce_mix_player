#pragma once

#include <mutex>

extern "C" {
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
}

struct GstVideoPlayerVars {
    GstElement* pipeline = nullptr;
    GstElement* sink = nullptr;
    void* windowHandle = nullptr; // native UIView* used for video rendering
    std::mutex mtx;
};

class GstPlayer {

public:
    GstPlayer();
    ~GstPlayer();

    bool setURL(std::string url);
    void setWindowHandle(void* nativeView);

    void play();
    void pause();
    void stop();
    void seek(double position);
    void dispose();

    // Public for bus callback access
    GstElement* pipeline = nullptr;

private:
    GstElement* sink = nullptr;
    GstVideoPlayerVars* p;
};
