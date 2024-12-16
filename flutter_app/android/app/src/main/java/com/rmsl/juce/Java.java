package com.rmsl.juce;

import android.content.Context;

public class Java
{
    static
    {
        System.loadLibrary ("juce_lib");
    }

    public native static void initialiseJUCE (Context appContext);
    public native static void juceMessageManagerInit ();
}