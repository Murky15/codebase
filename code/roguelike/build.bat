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
set ignore=-wd4146 -wd4244 -wd4005
set out=-Fe
set libs=User32.lib d3d11.lib dxgi.lib dxguid.lib D3DCompiler.lib Winmm.lib

set defines=-DWIN32_ROGUELIKE_SOURCE_PATH=\"%source%\" -DWIN32_ROGUELIKE_ASSET_PATH=\"%assets%\"

if not exist build mkdir build
pushd build
%compiler% %defines% %debug% %warn% %ignore% -I%source%.. %source%platform_win32.c %out%roguelike.exe %libs% || exit /b 1
popd