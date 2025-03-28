#!/bin/sh

# convert to SHARED library for android
scriptDir="$(dirname "$(realpath "$0")")"

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
' "$scriptDir/Builds/Android/lib/CMakeLists.txt" > "$scriptDir/Builds/Android/lib/CMakeLists_new.txt"

mv "$scriptDir/Builds/Android/lib/CMakeLists_new.txt" "$scriptDir/Builds/Android/lib/CMakeLists.txt"