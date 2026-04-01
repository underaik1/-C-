@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "BUILD_DIR=%ROOT%\build_ninja"
set "BUILD_TYPE=Release"
set "WEBVIEW2_VERSION=1.0.3856.49"
set "WEBVIEW2_ROOT=%USERPROFILE%\.nuget\packages\microsoft.web.webview2"

echo.
echo [1/7] Checking CMake...
call :ensure_tool cmake "Kitware.CMake" "C:\Program Files\CMake\bin\cmake.exe"
if errorlevel 1 goto :fail

echo [2/7] Checking Ninja...
call :ensure_tool ninja "Ninja-build.Ninja" "C:\Program Files\Ninja\ninja.exe"
if errorlevel 1 goto :fail

echo [3/7] Checking MSVC toolchain...
call :ensure_msvc
if errorlevel 1 goto :fail

echo [4/7] Checking WebView2 SDK...
call :ensure_webview2
if errorlevel 1 goto :fail

echo [5/7] Configuring project...
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 goto :fail

echo [6/7] Building project...
cmake --build "%BUILD_DIR%"
if errorlevel 1 goto :fail

echo [7/7] Done.
echo Build finished successfully: "%BUILD_DIR%\saper_webview.exe"
exit /b 0

:ensure_tool
set "TOOL_NAME=%~1"
set "WINGET_ID=%~2"
set "FALLBACK_EXE=%~3"

where %TOOL_NAME% >nul 2>nul
if not errorlevel 1 exit /b 0

echo %TOOL_NAME% not found in PATH. Trying winget install: %WINGET_ID%
where winget >nul 2>nul
if errorlevel 1 (
    echo winget is not available. Install %TOOL_NAME% manually.
    exit /b 1
)

winget install --id "%WINGET_ID%" --source winget --silent --accept-package-agreements --accept-source-agreements
if errorlevel 1 (
    echo Failed to install %TOOL_NAME% via winget.
    exit /b 1
)

if exist "%FALLBACK_EXE%" (
    for %%I in ("%FALLBACK_EXE%") do set "PATH=!PATH!;%%~dpI"
)

where %TOOL_NAME% >nul 2>nul
if errorlevel 1 (
    echo %TOOL_NAME% is still unavailable after installation.
    exit /b 1
)
exit /b 0

:ensure_msvc
set "VCVARS="

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do (
        if exist "%%~I" set "VCVARS=%%~I"
    )
)

if not defined VCVARS (
    for %%P in (
        "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) do (
        if not defined VCVARS if exist %%~P set "VCVARS=%%~P"
    )
)

if not defined VCVARS (
    echo MSVC Build Tools not found. Trying winget install...
    where winget >nul 2>nul
    if errorlevel 1 (
        echo winget is not available. Install Visual Studio Build Tools manually.
        exit /b 1
    )
    winget install --id Microsoft.VisualStudio.2022.BuildTools --source winget --silent --accept-package-agreements --accept-source-agreements --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if errorlevel 1 (
        echo Failed to install Visual Studio Build Tools.
        exit /b 1
    )

    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do (
            if exist "%%~I" set "VCVARS=%%~I"
        )
    )
)

if not defined VCVARS (
    echo Could not locate vcvars64.bat after installation.
    exit /b 1
)

call "%VCVARS%"
if errorlevel 1 (
    echo Failed to initialize MSVC environment.
    exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    echo cl.exe not found after vcvars initialization.
    exit /b 1
)
exit /b 0

:ensure_webview2
if not exist "%WEBVIEW2_ROOT%" mkdir "%WEBVIEW2_ROOT%"

set "HAS_WEBVIEW2="
for /d %%D in ("%WEBVIEW2_ROOT%\*") do (
    if exist "%%~fD\build\native\include\WebView2.h" set "HAS_WEBVIEW2=1"
)
if defined HAS_WEBVIEW2 exit /b 0

echo WebView2 SDK not found. Downloading version %WEBVIEW2_VERSION%...
set "NUPKG=%TEMP%\Microsoft.Web.WebView2.%WEBVIEW2_VERSION%.nupkg"
set "TARGET_DIR=%WEBVIEW2_ROOT%\%WEBVIEW2_VERSION%"

powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; Invoke-WebRequest -Uri 'https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/%WEBVIEW2_VERSION%' -OutFile '%NUPKG%'"
if errorlevel 1 (
    echo Failed to download WebView2 SDK package.
    exit /b 1
)

if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"

powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath '%NUPKG%' -DestinationPath '%TARGET_DIR%' -Force"
if errorlevel 1 (
    echo Failed to extract WebView2 SDK package.
    exit /b 1
)

del /q "%NUPKG%" >nul 2>nul

if not exist "%TARGET_DIR%\build\native\include\WebView2.h" (
    echo WebView2 SDK extraction finished but headers were not found.
    exit /b 1
)

exit /b 0

:fail
echo.
echo Build failed.
exit /b 1
