#!/bin/bash

x86_64-w64-mingw32-gcc -O0 -g -Wno-sign-compare -static -Wno-incompatible-pointer-types -Wno-override-init-side-effects -Wno-pointer-sign -Icode code/roguelike/roguelike.c -o build/roguelike.exe -luser32 -ld3d11 -ldxgi -ldxguid -ld3dcompiler -lwinmm