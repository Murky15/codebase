@echo off

tcc -Icode -I./code/dumb_level_editor/raylib/include ./code/dumb_level_editor/level_editor.c ./code/dumb_level_editor/raylib/lib/raylib.dll -o build/dumb_level_editor.exe