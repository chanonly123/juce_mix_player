package com.example.flutter_app;

import android.app.Application;
import com.rmsl.juce.Java;
import com.rmsl.juce.Native;

public class MyApp extends Application {

    @Override
    public void onCreate() {
        super.onCreate();
        Java.initialiseJUCE(this);
        Native.juceMessageManagerInit();
    }
}
