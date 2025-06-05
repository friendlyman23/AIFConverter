@echo off

REM — check for “-p” or “-P” and set PP_OUT accordingly
set "PP_OUT="
if /I "%~1"=="-p" (
    set "PP_OUT=-P"
)

set "COMMON_FLAGS=/W4 /WX /wd4200 /wd4201 /Od /Zi /nologo"

REM — create build dir if needed
if not exist build (
    mkdir build
)
pushd build

REM — compile hash module as C
REM cl /c /TC %COMMON_FLAGS% %PP_OUT% ..\code\GPerfHash.c

REM — compile and link Aif converter
REM - cl %COMMON_FLAGS% %PP_OUT% ..\code\win32_converter.cpp GPerfHash.obj user32.lib

REM — compile and link Wav converter
cl %COMMON_FLAGS% %PP_OUT% ..\code\win32_WavConverter.cpp user32.lib

popd

REM — cleanup
pushd build
    del *.obj
popd
