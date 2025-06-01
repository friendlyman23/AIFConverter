@echo off

REM — check for cl.exe
where /q cl || (
    echo ERROR: This program must be built using the cl compiler, 
    echo which is only accessible from the MSVC x64 Native Tools Command Prompt.
    exit /b 1
)

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
popd
