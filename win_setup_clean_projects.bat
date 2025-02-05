@echo off
set libname=juce_lib

:: rem Remove the Builds directory
:: rmdir /s /q "%libname%\Builds"

rem Generate native JUCE projects for iOS and Android
rem You can open JuceKit/%libname%.jucer using Projucer
"C:\JUCE\Projucer.exe" --resave "%libname%\%libname%.jucer"

if not exist "%libname%\Builds\iOS" (
    echo 🔴 Failed to create iOS lib project
    exit /b 1
)

if not exist "%libname%\Builds\Android" (
    echo 🔴 Failed to create Android lib project
    exit /b 1
)

echo ✅ JUCE project successfully created!