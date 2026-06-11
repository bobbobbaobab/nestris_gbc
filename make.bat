@echo off
setlocal enabledelayedexpansion

set LCC=..\..\..\bin\lcc
set OUT=GAME.gbc
set OBJDIR=obj
set OBJS=

REM include paths
set INCLUDES=-I. -Isrc -Igfx -Isnd -Iutils

REM clean old rom
DEL /Q *.gb 2>NUL
DEL /Q *.gbc 2>NUL

REM make obj dir if not exists
IF NOT EXIST %OBJDIR% mkdir %OBJDIR%

REM clean old obj files
DEL /Q %OBJDIR%\*.o 2>NUL
DEL /Q %OBJDIR%\*.asm 2>NUL
DEL /Q %OBJDIR%\*.lst 2>NUL
DEL /Q %OBJDIR%\*.sym 2>NUL
DEL /Q %OBJDIR%\*.ihx 2>NUL

REM compile src/*.c
FOR %%f IN (src\*.c) DO (
    echo Compiling %%f...
    %LCC% %INCLUDES% -c -o "%OBJDIR%\src_%%~nf.o" "%%f"
    IF ERRORLEVEL 1 GOTO error

    set OBJS=!OBJS! "%OBJDIR%\src_%%~nf.o"
)

REM compile gfx/*.c
FOR %%f IN (gfx\*.c) DO (
    echo Compiling %%f...
    %LCC% %INCLUDES% -c -o "%OBJDIR%\gfx_%%~nf.o" "%%f"
    IF ERRORLEVEL 1 GOTO error

    set OBJS=!OBJS! "%OBJDIR%\gfx_%%~nf.o"
)

REM compile snd/*.c
FOR %%f IN (snd\*.c) DO (
    echo Compiling %%f...
    %LCC% %INCLUDES% -c -o "%OBJDIR%\snd_%%~nf.o" "%%f"
    IF ERRORLEVEL 1 GOTO error

    set OBJS=!OBJS! "%OBJDIR%\snd_%%~nf.o"
)

REM compile utils/*.c
FOR %%f IN (utils\*.c) DO (
    echo Compiling %%f...
    %LCC% %INCLUDES% -c -o "%OBJDIR%\utils_%%~nf.o" "%%f"
    IF ERRORLEVEL 1 GOTO error

    set OBJS=!OBJS! "%OBJDIR%\utils_%%~nf.o"
)

REM check if there are object files
IF "!OBJS!"=="" (
    echo No .c files found.
    GOTO error
)

REM link all .o files
echo Linking %OUT%...
%LCC% -Wm-yC -o "%OUT%" !OBJS! "utils\hUGEDriver.lib"
IF ERRORLEVEL 1 GOTO error

DEL /Q *.ihx 2>NUL

echo.
echo Build success: %OUT%
EXIT /B 0

:error
echo.
echo Build failed.
EXIT /B 1