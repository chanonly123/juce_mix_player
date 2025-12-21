package com.example.flutter_app;

import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import io.flutter.embedding.android.FlutterActivity;
import io.flutter.embedding.engine.FlutterEngine;

public class MainActivity extends FlutterActivity {

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void configureFlutterEngine(@NonNull FlutterEngine flutterEngine) {
        super.configureFlutterEngine(flutterEngine);

        flutterEngine.getPlatformViewsController().getRegistry()
                .registerViewFactory("gst-video-view", new GstVideoPlayerView.Factory());

        flutterEngine.getPlatformViewsController().getRegistry()
                .registerViewFactory("unified-video-view", new UnifiedVideoView.Factory());
    }
}
