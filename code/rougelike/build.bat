@echo off
cl -nologo -Zi -wd4005 -Icode code/rougelike/rougelike.c -Fobuild/ -Fdbuild/ -Febuild/rougelike.exe User32.lib d3d11.lib dxgi.lib dxguid.lib D3DCompiler.lib