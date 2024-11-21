package com.example.flutter_app

import com.rmsl.juce.Java
import io.flutter.app.FlutterApplication

class MyApp: FlutterApplication() {

    override fun onCreate() {
        super.onCreate()
        Java.initialiseJUCE (this)
    }
}