@echo off

set "COMMON_FLAGS=/W4 /WX /wd4200 /O2 /MT /nologo"

REM — create build dir if needed
if not exist build (
    mkdir build
)
pushd build

REM — compile hash module as C
cl /c /TC %COMMON_FLAGS% %PP_OUT% ..\code\GPerfHash.c

REM — compile and link
cl %COMMON_FLAGS% %PP_OUT% ..\code\win32_converter.cpp GPerfHash.obj user32.lib

popd

REM — cleanup
pushd build
    del *.obj
