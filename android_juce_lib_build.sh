# returns immediately if any command returns error
set -e

libname="juce_lib"
flutter_app="flutter_app"

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

# go to gradle project
cd "$libname/Builds/Android/lib" &&

# clean build dir
if [[ "$2" = "-clean" ]]; then
    echo "✅ cleaning build directory"
    gradle clean
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
' "CMakeLists.txt" > CMakeLists_new.txt

mv CMakeLists_new.txt CMakeLists.txt

# gradle build
if $debug; then
    gradle assembleDebug --debug
else
    gradle assembleRelease
fi

echo "✅ Build Success [debug $debug] ✅"

cd "$root_dir"

# find build output directory
if $debug; then
    libDirectory=`realpath | find . -type d -path "*/$libname/*/cxx/Debug/*/obj"`
else
    libDirectory=`realpath | find . -type d -path "*/$libname/*/stripped_native_libs/release_Release/*/lib"`
fi

if [ ! -d "$libDirectory" ]; then
    echo "🔴 build output binary directory not found!"
    exit 1
else 
    echo "✅ build directory found [$libDirectory]"
fi

if [ -d "$flutter_app/android/app/src/main" ]; then

    rm -rf "$flutter_app/android/app/src/main/jniLibs"
    
    cp -r $libDirectory "$flutter_app/android/app/src/main/jniLibs/" &&

    echo "✅ Copy to [$flutter_app] project Success ✅"
else
    echo "🔴 [$flutter_app/android/app/src/main] not found ✅"
    exit 1
fi