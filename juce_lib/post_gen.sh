#!/bin/sh

# convert to SHARED library for android
scriptDir="$(dirname "$(realpath "$0")")"

CMAKE_FILE="$scriptDir/Builds/Android/lib/CMakeLists.txt"
INSERT_FILE="$scriptDir/gstremer_cmake_import.txt"
TMP_FILE="$scriptDir/CMakeLists_temp.txt"

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
' "$CMAKE_FILE" > "$TMP_FILE"

mv "$TMP_FILE" "$CMAKE_FILE"

# inserts gstreamer android libarry to the CMakeLists.txt

awk -v insert_file="$INSERT_FILE" '
/target_link_libraries/ && !done {
    while ((getline line < insert_file) > 0)
        print line
    close(insert_file)
    done=1
}
{ print }
' "$CMAKE_FILE" > "$TMP_FILE"

mv "$TMP_FILE" "$CMAKE_FILE"