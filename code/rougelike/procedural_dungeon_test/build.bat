@echo off

set RLPATH=.\raylib
cl -nologo -wd4005 -I..\..\ -I%RLPATH%\include test.c -link %RLPATH%\lib\raylibdll.lib