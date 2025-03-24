#!/bin/sh

# convert to SHARED library for android
currentDir="$(dirname "$(realpath "$0")")"

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
' "$currentDir/Builds/Android/lib/CMakeLists.txt" > "$currentDir/Builds/Android/lib/CMakeLists_new.txt"

mv "$currentDir/Builds/Android/lib/CMakeLists_new.txt" "$currentDir/Builds/Android/lib/CMakeLists.txt"