@echo off

REM — if we want preprocessor output
set "PP_OUT="
if /I "%~1"=="-p" (
    set "PP_OUT=-P"
)

set "COMMON_FLAGS=/W4 /WX /nologo"
set "DEBUG_FLAGS=/wd4200 /wd4201 /wd4189 /Od /Zi -DDEBUG"

REM - disabled compiler warnings (above) in debug mode:
REM - 4189(local variable is initialized but not referenced) 
REM - 4200(struct with zero-size array member) 
REM - 4201(nameless struct or union)

REM — create build dir if needed
if not exist ..\build (
    mkdir ..\build
)
pushd ..\build

REM — compile hash module as C
REM cl /c /TC %COMMON_FLAGS% %PP_OUT% ..\code\GPerfHash.c

REM — compile and link Aif converter
REM - cl %COMMON_FLAGS% %PP_OUT% ..\code\win32_converter.cpp GPerfHash.obj user32.lib

REM — compile and link Wav converter
cl %COMMON_FLAGS% %DEBUG_FLAGS% %PP_OUT% ..\code\win32_WavConverter.cpp user32.lib

popd

REM — cleanup
pushd ..\build
    del *.obj
popd
