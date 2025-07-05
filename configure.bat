@echo off

:: Clean previous build
if exist "_build" (
	echo Removing old build directory...
	rmdir /s /q _build
)

:: Create fresh build directory
mkdir _build

echo Configuring build...

:: Run CMake
cmake -S . -B _build %1 %2 %3
if %ERRORLEVEL% neq 0 (
	echo Error: CMake failed to configure the project.
	pause
	exit /b %ERRORLEVEL%
)

echo Done configuring the project.

pause
