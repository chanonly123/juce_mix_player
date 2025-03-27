@echo off
setlocal enabledelayedexpansion

set "currentDir=%~dp0"
set "input_file=%currentDir%Builds\Android\lib\CMakeLists.txt"
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
