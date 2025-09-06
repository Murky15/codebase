@echo off
cl -nologo -Icode -wd4005 -wd4028 ./code/letterman_project_umich/letterman.c -Fobuild/ -Fdbuild/ -Febuild/letterman.exe