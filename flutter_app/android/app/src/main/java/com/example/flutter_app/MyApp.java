package com.example.flutter_app;

import com.rmsl.juce.Java;

import io.flutter.app.FlutterApplication;

public class MyApp extends FlutterApplication {

    public native static void juceMessageManagerInit ();

    @Override
    public void onCreate() {
        super.onCreate();
        Java.initialiseJUCE(this);
        MyApp.juceMessageManagerInit();
    }
}
