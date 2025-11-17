@echo off
setlocal enableextensions
set source=%~dp0

set compiler=cl -nologo
set debug=-Od -Zi
set warn=-WX -W3
set ignore=-wd4146 -wd4244 -wd4005
set out=-Fe
set libs=User32.lib d3d11.lib dxgi.lib dxguid.lib D3DCompiler.lib Winmm.lib

if not exist build mkdir build
pushd build
%compiler% %debug% %warn% %ignore% -I%source%../ %source%/roguelike.c %out%roguelike.exe %libs%
popd