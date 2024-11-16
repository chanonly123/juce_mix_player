target="juce_lib - Static Library"
libname="juce_lib"

rootDir=`pwd`

cd "$libname/Builds/iOS" &&

rm -rf build

xcodebuild -target "${target}" -configuration Release -sdk iphoneos only_active_arch=no build LLVM_LTO=NO &&

mkdir build/Release-iphoneos &&
mv "build/Release/lib${libname}.a" "build/Release-iphoneos/${libname}.a" &&

xcodebuild -target "${target}" -configuration Release -sdk iphonesimulator only_active_arch=no LLVM_LTO=NO &&

mkdir build/Release-iphonesimulator &&
mv "build/Release/lib${libname}.a" "build/Release-iphonesimulator/${libname}.a" &&

xcodebuild -create-xcframework \
    -library "build/Release-iphoneos/${libname}.a" -headers ../../../cpp_source/includes \
    -library "build/Release-iphonesimulator/${libname}.a" -headers ../../../cpp_source/includes \
    -output "build/${libname}.xcframework" &&

echo "✅ Build Success ✅"

cd "$rootDir"

# copy framework to flutter_app project

if [ -d "flutter_app/ios/Runner/" ]; then
    rm -rf "flutter_app/ios/Runner/$libname.xcframework"

    cp -r \
    "$libname/Builds/iOS/build/$libname.xcframework" \
    "flutter_app/ios/Runner/$libname.xcframework" &&

    echo "✅ Copy to project Success ✅"
fi