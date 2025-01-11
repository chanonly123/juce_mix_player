#!/bin/bash

libname="juce_lib"
libDirectory=`realpath | find . -type d -path "*/$libname/*/RelWithDebInfo/*/obj/arm64-v8a"`

if [ ! -d "$libDirectory" ]; then
    echo "🔴 build output binary directory not found!"
    exit 1
else 
    echo "✅ build directory found [$libDirectory]"
fi

$NDK/ndk-stack -sym "$libDirectory" -dump "crash.log"