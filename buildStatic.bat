:: Define "SeeColor" too enable untested fully auto ticket taking by looking at specific pixel colors

windres -i .\src\resource.rc -o .\build\resource.o
gcc -DINTERCEPTION_STATIC -Wall -c .\src\PressF3Interception.c -o .\build\PressF3Interception.o
gcc .\build\PressF3Interception.o .\build\resource.o -o .\build\PressF3Interception.exe -l:interception.a -luser32 -lgdi32 -lshell32 -lpsapi -mwindows

pause