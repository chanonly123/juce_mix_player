package com.example.flutter_app

import android.os.Bundle
import com.rmsl.juce.Java
import io.flutter.embedding.android.FlutterActivity

class MainActivity() : FlutterActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Java.initialiseJUCE (this.applicationContext)
    }
}
