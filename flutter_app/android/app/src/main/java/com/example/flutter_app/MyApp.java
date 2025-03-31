package com.example.flutter_app;

import android.app.Application;
import com.rmsl.juce.Java;

public class MyApp extends Application {

    @Override
    public void onCreate() {
        super.onCreate();
        Java.initialiseJUCE(this);
    }
}
