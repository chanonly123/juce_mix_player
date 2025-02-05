libname="juce_lib"

# rm -rf "$libname/Builds"

# generate native juce projects for iOS and Android
# you can open JuceKit/$libname.jucer using projucer
~/JUCE/Projucer.app/Contents/MacOS/Projucer --resave "$libname/$libname.jucer"

if [ ! -d "$libname/Builds/iOS" ]; then
    echo "🔴 failed to create iOS lib project"
    exit 1
fi

if [ ! -d "$libname/Builds/Android" ]; then
    echo "🔴 failed to create iOS lib project"
    exit 1
fi