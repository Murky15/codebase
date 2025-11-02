@echo off
cl -Od -nologo -Zi -WX -W3 -wd4146 -wd4244 -wd4005 -Icode code/roguelike/roguelike.c -Fobuild/ -Fdbuild/ -Febuild/roguelike.exe User32.lib d3d11.lib dxgi.lib dxguid.lib D3DCompiler.lib