windres -i .\src\resource.rc -o .\build\resource.o
gcc -Wall -c .\src\PressF3Interception.c -o .\build\PressF3Interception.o
gcc .\build\PressF3Interception.o .\build\resource.o -o .\build\PressF3Interception.exe -linterception -luser32 -lgdi32 -lshell32 -lpsapi -mwindows

pause