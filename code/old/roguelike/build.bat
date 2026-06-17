@echo off
setlocal enableextensions
set source=%~dp0
set source=%source:\=/%
if not exist assets (
  echo This script must be run from the root directory!
  exit /b 1
) else (
  set assets=%cd%assets\roguelike\
)
set assets=%assets:\=/%

set compiler=cl -nologo
set debug=-Od -Zi
set warn=-WX -W3
set ignore=-wd4146 -wd4244 -wd4005 -wd4309
set out=-Fe
set defines=-DWIN32_ROGUELIKE_SOURCE_PATH_WIDE=L\"%source%\" -DWIN32_ROGUELIKE_ASSET_PATH_WIDE=L\"%assets%\"
set defines=%defines% -DWIN32_ROGUELIKE_SOURCE_PATH=\"%source%\" -DWIN32_ROGUELIKE_ASSET_PATH=\"%assets%\"


set compile=%compiler% %defines% %debug% %warn% %ignore% -I%source%..
set platform_opts=-std:c++20
set platform_link=-link -SUBSYSTEM:WINDOWS User32.lib d3d11.lib dxgi.lib dxguid.lib D3DCompiler.lib Winmm.lib Shlwapi.lib Ole32.lib
set game_link=-link -INCREMENTAL:NO -DLL -EXPORT:roguelike_init -EXPORT:roguelike_tick -EXPORT:roguelike_draw -EXPORT:roguelike_audio_callback

if not exist build mkdir build
pushd build
echo Building game dll...
%compile% %source%roguelike.c %out%roguelike_new.dll %game_link% || exit /b 1
echo Building platform layer...
%compile% %platform_opts% %source%platform_win32.cpp %out%roguelike_platform_win32.exe %platform_link% || exit /b 1
echo Done
popd