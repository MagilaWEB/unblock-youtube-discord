@echo off
setlocal

:: Set default build type
set BUILD_TYPE=Release
if not "%1"=="" (
    set BUILD_TYPE=%1
)

if not exist "_build" (
    echo Error: Build directory doesn't exist. Run configure.bat first!
    exit /b 1
)

echo Building %BUILD_TYPE% version...

:: Run CMake
cmake --build _build --config %BUILD_TYPE% --parallel || (
    echo Build failed!
    pause
    exit /b 1
)

echo Build completed successfully!\nBinaries available in _build\bin

pause
endlocal
