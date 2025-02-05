package com.example.flutter_app;

import com.rmsl.juce.Java;
import com.rmsl.juce.Native;
import io.flutter.app.FlutterApplication;

public class MyApp extends FlutterApplication {

    @Override
    public void onCreate() {
        super.onCreate();
        Java.initialiseJUCE(this);
        Native.juceMessageManagerInit();
    }
}
