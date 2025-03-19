@echo off

mkdir build
pushd build
cl -Zi -Od ..\code\win32_converter.cpp user32.lib
popd
