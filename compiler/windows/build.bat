@echo off
REM Build Tempo Compiler for Windows - 100% Assembly

echo Building Tempo Compiler for Windows...

REM Assemble
nasm -f win64 tempo-compiler.asm -o tempo-compiler.obj

REM Link with kernel32
link /subsystem:console /entry:WinMainCRTStartup tempo-compiler.obj kernel32.lib

echo Built tempo-compiler.exe for Windows

REM Test
if exist tempo-compiler.exe (
    echo Testing Windows compiler...
    tempo-compiler.exe hello.tempo
)