@echo off

cl -nologo -FC -J -I./code -EHa- -GR- -Od -Zi -WX -W3 -wd4146 -wd4005 -wd4101 -wd4267 -wd4244 -wd4996 -DDEBUG=1 -DENABLE_ASSERT=1 ./code/dumb/platform_win32_multithreaded.c User32.lib Gdi32.lib Opengl32.lib Winmm.lib -Fo./build/ -Fd./build/ -Fe./build/dumb.exe