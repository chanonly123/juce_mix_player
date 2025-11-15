#pragma once

extern "C" {
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
}

struct GstVideoPlayerVars {
    GstElement* pipeline = nullptr;
    GstElement* sink = nullptr;
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
    void dispose();

private:
    GstElement* pipeline = nullptr;
    GstElement* sink = nullptr;
    
    GstVideoPlayerVars* p;
};
