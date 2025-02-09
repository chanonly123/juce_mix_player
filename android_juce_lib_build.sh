# returns immediately if any command returns error
set -e

libname="juce_lib"
flutter_app="flutter_app"
flutter_wrapper_package="juce_mix_player_package"

# check for options
if [ "$1" = "-debug" ]; then
    debug=true;
elif [ "$1" = "-release" ]; then
    debug=false;
else
    echo "Usage: command -debug/release -clean"
    exit 1
fi

root_dir=`pwd`

# generate dart files from native header
cd "$flutter_wrapper_package"
flutter pub get
dart run ffigen

# go to gradle project
cd "$root_dir"
cd "$libname/Builds/Android" &&

# clean build dir
if [[ "$2" = "-clean" ]]; then
    echo "✅ cleaning build directory"
    ./gradlew clean
    rm -rf build
else
    echo "✅ using cached build directory"
fi

# convert to SHARED library for android
awk '
{
    # Trim leading and trailing spaces
    gsub(/^ +| +$/, "", $0);

    if ($0 == "STATIC") {
        print "SHARED"; 
    } else {
        print $0;
    }
}
' "lib/CMakeLists.txt" > lib/CMakeLists_new.txt

mv lib/CMakeLists_new.txt lib/CMakeLists.txt

# gradle build
if $debug; then
    ./gradlew assembleDebug --debug
else
    ./gradlew assembleRelease
fi

echo "✅ Build Success [debug $debug] ✅"

cd "$root_dir"

# find build output directory
if $debug; then
    libOutFile="$libname/Builds/Android/lib/build/outputs/aar/lib-debug_-debug.aar"
else
    libOutFile="$libname/Builds/Android/lib/build/outputs/aar/lib-release_-release.aar"
fi

if [ ! -f "$libOutFile" ]; then
    echo "🔴 build output binary not found!"
    exit 1
else 
    echo "✅ build output found [$libOutFile]"
fi

cd "$root_dir"

rm -rf \
"dist/$libname.aar"

cp -r \
"$libOutFile" \
"dist/$libname.aar"

echo "✅ Copy arr to 'dist' success ✅"