@echo off

REM — set preprocessor output option if requested
set "PP_OUT="
if /I "%~1"=="-p" (
    set "PP_OUT=-P"
)

set "COMMON_FLAGS=/W4 /WX /wd4200 -Zi -Od /nologo"

REM — create build dir if needed
if not exist build (
    mkdir build
)
pushd build

REM — compile hash module as C
cl /c /TC %COMMON_FLAGS% %PP_OUT% ..\code\GPerfHash.c

cl %COMMON_FLAGS% %PP_OUT% ..\code\win32_converter.cpp GPerfHash.obj user32.lib

popd

REM — clean out any old WAVs
pushd data
del *.wav
popd
