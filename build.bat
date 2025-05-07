@echo off
REM — check for “-p” or “-P” and set PP_OUT accordingly
set "PP_OUT="
if /I "%~1"=="-p" (
    set "PP_OUT=-P"
)

REM — create build dir if needed, then compile
if not exist build (
    mkdir build
)
pushd build
cl -Zi -Od %PP_OUT% ..\code\win32_converter.cpp user32.lib
popd

REM — clean out any old WAVs
pushd data
del *.wav
popd
