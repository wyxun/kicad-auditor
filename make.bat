@echo off
set PATH=D:\software\msys64\mingw64\bin;%PATH%
echo [BUILD] Triggering Makefile build using mingw32-make...
mingw32-make clean
mingw32-make
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    exit /b %errorlevel%
)
echo [SUCCESS] Build completed successfully.
echo.
echo ==========================================
echo [TEST 1] Running global help test
echo ==========================================
.\kicad-auditor.exe
echo.
echo ==========================================
echo [TEST 2] Running command help test
echo ==========================================
.\kicad-auditor.exe run --help
echo.
echo ==========================================
echo [TEST 3] Running geometric engine self-test
echo ==========================================
.\kicad-auditor.exe test
