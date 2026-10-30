@echo off
setlocal
:: Locate MinGW-w64 toolchain
set CLANG=C:/mingw64/bin/clang.exe
if not exist "%CLANG%" set CLANG=C:/mingw64/mingw64/bin/clang.exe
set WINDRES=C:/mingw64/bin/windres.exe
if not exist "%WINDRES%" set WINDRES=C:/mingw64/mingw64/bin/windres.exe

if not exist "%CLANG%" (
    echo [ERROR] clang.exe not found. Install MinGW-w64 and add its bin to PATH.
    goto :eof
)

:: 1) Compile resources (icon only; NO manifest embedded)
"%WINDRES%" resource.rc -O coff -o resource.o
if errorlevel 1 (
    echo [FAIL] resource compile error
    goto :eof
)

:: 2) Compile + link.
::    -O2                 normal optimization (no aggressive size tricks)
::    -nostartfiles       skip CRT startup, use custom entry app_entry
::                        -> avoids UCRT (api-ms-win-crt-*) which does NOT exist on Win7
::    -Wl,-e,app_entry    custom entry point (see regcode.c)
::    no manifest         -> avoids Win7 "side-by-side configuration incorrect" (SxS) error
::    -lgdi32 -luser32 -lkernel32   system DLLs only, Win7~Win11 compatible
"%CLANG%" -O2 -mwindows -nostartfiles -static -Wl,-e,app_entry regcode.c resource.o -o regcode.exe -lgdi32 -luser32 -lkernel32
if errorlevel 1 (
    echo [FAIL] program compile error
    goto :eof
)

echo [OK] regcode.exe built (system DLLs only, runs on Win7~Win11)
endlocal
