@echo off
setlocal

echo ===================================================
echo Building RamCpp4 (Release x64) - Visual Studio 2026
echo ===================================================

set MSBUILD="D:\MyPrograms\VisualStudio\MSBuild\Current\Bin\MSBuild.exe"

if not exist %MSBUILD% (
    for /f "tokens=*" %%i in ('where msbuild 2^>nul') do (
        set MSBUILD="%%i"
        goto :found_msbuild
    )
    echo [ERROR] MSBuild.exe not found
    exit /b 1
)

:found_msbuild
echo Using MSBuild: %MSBUILD%

%MSBUILD% "%~dp0RamCpp4.slnx" /p:Configuration=Release /p:Platform=x64 /nologo /m
if errorlevel 1 goto :build_failed

echo.
echo [SUCCESS] RamCpp4 built successfully with static runtime (/MT).
echo Output directory: %~dp0x64\Release\
echo.
echo Portable distribution ready at: %~dp0ReleaseFiles\
dir /b "%~dp0ReleaseFiles\"
goto :eof

:build_failed
echo.
echo [FAILED] Build failed with error code %ERRORLEVEL%.
exit /b %ERRORLEVEL%
