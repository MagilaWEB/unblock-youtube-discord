@echo off

set BUILD_DIR=_build_ai

:: Clean previous build
if exist "%BUILD_DIR%" (
	echo Removing old build directory...
	rmdir /s /q %BUILD_DIR%
)

:: Create fresh build directory
mkdir %BUILD_DIR%

echo Configuring build...

:: Run CMake with Ninja + Clang preset
cmake -S . -B %BUILD_DIR% --preset debug %1 %2 %3
if %ERRORLEVEL% neq 0 (
	echo Error: CMake failed to configure the project.
	pause
	exit /b %ERRORLEVEL%
)

echo Done configuring the project.

pause
