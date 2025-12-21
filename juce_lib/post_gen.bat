@echo off
setlocal enabledelayedexpansion

set "scriptDir=%~dp0"
set "input_file=%scriptDir%Builds\Android\lib\CMakeLists.txt"
set "temp_file=%input_file%_out.txt"

if not exist "%input_file%" (
    echo File not found: %input_file%
    exit /b 1
)

> "%temp_file%" (
    for /f "tokens=* delims=" %%a in (%input_file%) do (
        set "line=%%a"
        set "trimmed=!line: =!"

        if "!trimmed!"=="STATIC" (
            echo SHARED
        ) else (
            echo !line!
        )
    )
)

move /y "%temp_file%" "%input_file%" > nul
echo Processing complete.

:: Patch build.gradle to use c++_shared instead of c++_static (required for GStreamer)
set "gradle_file=%scriptDir%Builds\Android\lib\build.gradle"
if exist "%gradle_file%" (
    powershell -Command "(Get-Content '%gradle_file%') -replace 'c\+\+_static', 'c++_shared' | Set-Content '%gradle_file%'"
    echo Patched build.gradle: c++_static -^> c++_shared for GStreamer compatibility
)
