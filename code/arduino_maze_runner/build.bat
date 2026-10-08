@echo off
setlocal enableextensions

REM Context collection
set source=%~dp0
if not exist assets (
  echo This script must be run from the root directory!
  exit /b 1
) else (
  set third_party=%cd%code\third_party
  set assets=%cd%assets\
)

REM Libraries
set sdl3=%third_party%\SDL3-3.4.14\
set sdl3ttf=%third_party%\SDL3_ttf-3.2.2\
set sdl3img=%third_party%\SDL3_image-3.4.4\

REM Compiler options
set common=-nologo -std:c11 -EHa- -GR-
set debug=-DDEBUG=1 -DBASE_ENABLE_ASSERT=1 -Od -Zi -WX -W2
set ignored_warnings=-wd4146 -wd4042
set includes=-I%cd%\code -I%sdl3%include\ -I%sdl3ttf%include\ -I%sdl3img%include\
set libs=%sdl3%lib\x64\SDL3.lib %sdl3ttf%lib\x64\SDL3_ttf.lib %sdl3img%lib\x64\SDL3_image.lib

REM set system_libs=dxcompiler.lib
set system_libs=
set link_options=-SUBSYSTEM:WINDOWS -INCREMENTAL:NO

if not exist build mkdir build
pushd build
cl %common% %debug% %ignored_warnings% %includes% %source%main.cpp -FeRunner.exe -link %link_options% %libs% %system_libs% || exit /b 1
popd