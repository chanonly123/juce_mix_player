@echo off
setlocal enabledelayedexpansion

:: Set variables
set libname=juce_lib
set flutter_app=flutter_app
set flutter_wrapper_package=juce_mix_player_package

:: Check for options
if "%1"=="-debug" (
    set debug=true
) else if "%1"=="-release" (
    set debug=false
) else (
    echo Usage: command -debug/release -clean
    exit /b 1
)

:: Get the root directory
set root_dir=%cd%

:: Generate Dart files from native header
:: Check if flutter is installed and available
where flutter >nul 2>nul
if %errorlevel% neq 0 (
    echo 🔴 Flutter command not found! Please install Flutter and ensure it is in your PATH.
) else (
    cd %flutter_wrapper_package%
    flutter pub get
    dart run ffigen
)

:: Go to the Gradle project
cd %root_dir%
cd %libname%\Builds\Android\lib

:: Clean build directory
if "%2"=="-clean" (
    echo ✅ cleaning build directory
    gradle clean
    rmdir /s /q build
) else (
    echo ✅ using cached build directory
)

:: Convert to SHARED library for Android (not working)
powershell -Command "(Get-Content CMakeLists.txt) -replace '^STATIC$', 'SHARED' | Set-Content CMakeLists_new.txt"
move /y CMakeLists_new.txt CMakeLists.txt

:: Gradle build
if %debug%==true (
    gradle assembleDebug --debug
) else (
    gradle assembleRelease
)

echo ✅ Build Success [debug %debug%] ✅

:: Return to root directory
cd %root_dir%

:: Find build output directory
if %debug%==true (
    for /f "delims=" %%i in ('dir /s /b *\%libname%\*\cxx\Debug\*\obj') do set libDirectory=%%i
) else (
    for /f "delims=" %%i in ('dir /s /b *\%libname%\*\stripped_native_libs\release_Release\*\lib') do set libDirectory=%%i
)

if not defined libDirectory (
    echo 🔴 build output binary directory not found!
    exit /b 1
) else (
    echo ✅ build directory found [%libDirectory%]
)

:: Copy to dist folder


endlocal