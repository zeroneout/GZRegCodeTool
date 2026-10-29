@echo off
setlocal
:: 自动定位 MinGW-w64 工具链（你配 PATH 时解压到的位置）
set CLANG=C:\mingw64\bin\clang.exe
if not exist "%CLANG%" set CLANG=C:\mingw64\mingw64\bin\clang.exe
set WINDRES=C:\mingw64\bin\windres.exe
if not exist "%WINDRES%" set WINDRES=C:\mingw64\mingw64\bin\windres.exe

if not exist "%CLANG%" (
    echo [错误] 找不到 clang.exe，请确认 MinGW-w64 已安装并配置 PATH
    goto :eof
)

:: 1) 编译资源（图标 + 清单）-> resource.o
"%WINDRES%" resource.rc -O coff -o resource.o
if errorlevel 1 (
    echo [失败] 资源编译出错
    goto :eof
)

:: 2) 编译主程序并链接资源
::    关键体积优化（从 ~31KB 压到 ~13KB）：
::      -Os                 按体积而非速度优化
::      -nostartfiles       去掉 C 运行时(CRT)启动代码，改用自定义入口 app_entry
::      -mwindows           生成 GUI 子系统（关键：否则会弹出黑色控制台窗口）
::      -ffunction-sections -fdata-sections + --gc-sections  丢弃未使用的代码/数据段
::      -Wl,-e,app_entry    指定自定义入口（见 regcode.c）
::      -lgdi32 -luser32 -lkernel32  仅依赖系统自带 DLL（不使用 msvcrt/UCRT，兼容 Win7）
"%CLANG%" -Os -s -static -nostartfiles -mwindows -ffunction-sections -fdata-sections ^
    -Wl,--gc-sections -Wl,-e,app_entry regcode.c resource.o -o regcode.exe ^
    -lgdi32 -luser32 -lkernel32
if errorlevel 1 (
    echo [失败] 程序编译出错
    goto :eof
)

echo [成功] regcode.exe 已生成（约 13KB，仅依赖系统自带 DLL，Win7~Win11 可直接运行）
endlocal
