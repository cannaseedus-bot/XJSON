@echo off
setlocal enabledelayedexpansion

:: ─────────────────────────────────────────────────────────────────────────────
:: runtime.bat  --  json-runtime build + management helper
::
:: Commands:
::   runtime build [Release|Debug]      cmake configure + build all targets
::   runtime json   [args...]           run json_runtime.exe
::   runtime sco    [args...]           run sco_runtime.exe
::   runtime scxq2  [args...]           run scxq2_runtime.exe
::   runtime start                      start all three runtimes (separate windows)
::   runtime stop                       kill all three runtime processes
::   runtime status                     show which runtimes are running
::   runtime roundtrip <program.json>   scxq2 roundtrip verification
::   runtime help                       show this help
:: ─────────────────────────────────────────────────────────────────────────────

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build-asx"
set "CONFIG=Release"

if "%1"=="" goto :cmd_help
if /i "%1"=="help"      goto :cmd_help
if /i "%1"=="build"     goto :cmd_build
if /i "%1"=="json"      goto :cmd_json
if /i "%1"=="sco"       goto :cmd_sco
if /i "%1"=="scxq2"     goto :cmd_scxq2
if /i "%1"=="start"     goto :cmd_start
if /i "%1"=="stop"      goto :cmd_stop
if /i "%1"=="status"    goto :cmd_status
if /i "%1"=="roundtrip" goto :cmd_roundtrip

echo [runtime] Unknown command: %1
goto :cmd_help

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_build
if not "%2"=="" set "CONFIG=%2"
echo [runtime] Configuring cmake (config=%CONFIG%) ...
cmake -B "%BUILD_DIR%" -G "Visual Studio 17 2022" "%ROOT%" 2>&1
if errorlevel 1 ( echo [runtime] cmake configure failed & exit /b 1 )

echo [runtime] Building all targets ...
cmake --build "%BUILD_DIR%" --config %CONFIG% 2>&1
if errorlevel 1 ( echo [runtime] build failed & exit /b 1 )

echo [runtime] Build complete.
echo   json_runtime  : %BUILD_DIR%\%CONFIG%\json_runtime.exe
echo   sco_runtime   : %BUILD_DIR%\%CONFIG%\sco_runtime.exe
echo   scxq2_runtime : %BUILD_DIR%\%CONFIG%\scxq2_runtime.exe
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_json
set "EXE=%BUILD_DIR%\%CONFIG%\json_runtime.exe"
if not exist "%EXE%" (
    echo [runtime] json_runtime.exe not found -- run: runtime build
    exit /b 1
)
cd /d "%ROOT%"
"%EXE%" %2 %3 %4 %5 %6 %7 %8 %9
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_sco
set "EXE=%BUILD_DIR%\%CONFIG%\sco_runtime.exe"
if not exist "%EXE%" (
    echo [runtime] sco_runtime.exe not found -- run: runtime build
    exit /b 1
)
cd /d "%ROOT%"
"%EXE%" %2 %3 %4 %5 %6 %7 %8 %9
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_scxq2
set "EXE=%BUILD_DIR%\%CONFIG%\scxq2_runtime.exe"
if not exist "%EXE%" (
    echo [runtime] scxq2_runtime.exe not found -- run: runtime build
    exit /b 1
)
cd /d "%ROOT%"
"%EXE%" %2 %3 %4 %5 %6 %7 %8 %9
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_start
echo [runtime] Starting runtimes in separate windows ...

set "JEXE=%BUILD_DIR%\%CONFIG%\json_runtime.exe"
set "SEXE=%BUILD_DIR%\%CONFIG%\sco_runtime.exe"

if exist "%JEXE%" (
    start "json_runtime  [:8787]" /D "%ROOT%" "%JEXE%" --manifest manifest.json
    echo [runtime] json_runtime  started  (port 8787)
) else (
    echo [runtime] json_runtime.exe not found, skipping
)

if exist "%SEXE%" (
    start "sco_runtime   [:6002]" /D "%ROOT%" "%SEXE%" sco/registry.json --port 6002
    echo [runtime] sco_runtime   started  (port 6002)
) else (
    echo [runtime] sco_runtime.exe not found, skipping
)

echo [runtime] Use "runtime stop" to terminate all.
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_stop
echo [runtime] Stopping runtimes ...
taskkill /F /IM json_runtime.exe  2>nul && echo   stopped json_runtime
taskkill /F /IM sco_runtime.exe   2>nul && echo   stopped sco_runtime
taskkill /F /IM scxq2_runtime.exe 2>nul && echo   stopped scxq2_runtime
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_status
echo [runtime] Process status:
tasklist /FI "IMAGENAME eq json_runtime.exe"  2>nul | findstr json_runtime  && echo   json_runtime  RUNNING  || echo   json_runtime  stopped
tasklist /FI "IMAGENAME eq sco_runtime.exe"   2>nul | findstr sco_runtime   && echo   sco_runtime   RUNNING  || echo   sco_runtime   stopped
tasklist /FI "IMAGENAME eq scxq2_runtime.exe" 2>nul | findstr scxq2_runtime && echo   scxq2_runtime RUNNING  || echo   scxq2_runtime stopped

echo.
echo [runtime] Port check:
netstat -ano 2>nul | findstr ":8787 " | findstr LISTENING && echo   :8787 open (json_runtime) || echo   :8787 closed
netstat -ano 2>nul | findstr ":6002 " | findstr LISTENING && echo   :6002 open (sco_runtime)  || echo   :6002 closed
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_roundtrip
if "%2"=="" (
    echo Usage: runtime roundtrip ^<program.json^>
    exit /b 1
)
set "EXE=%BUILD_DIR%\%CONFIG%\scxq2_runtime.exe"
if not exist "%EXE%" (
    echo [runtime] scxq2_runtime.exe not found -- run: runtime build
    exit /b 1
)
cd /d "%ROOT%"
"%EXE%" --roundtrip %2
goto :eof

:: ─────────────────────────────────────────────────────────────────────────────
:cmd_help
echo.
echo  runtime.bat -- json-runtime build ^& management
echo.
echo  Commands:
echo    runtime build [Release^|Debug]       cmake configure + build all targets
echo    runtime json   [args...]             run json_runtime.exe
echo    runtime sco    [args...]             run sco_runtime.exe  (REST on :6002)
echo    runtime scxq2  [args...]             run scxq2_runtime.exe
echo    runtime start                        start json + sco runtimes (new windows)
echo    runtime stop                         kill all three runtime processes
echo    runtime status                       show running/stopped + port check
echo    runtime roundtrip ^<program.json^>    SCXQ2 compile+decompile lossless check
echo    runtime help                         this help
echo.
echo  Examples:
echo    runtime build
echo    runtime sco --list
echo    runtime sco --resolve OLLAMA_API
echo    runtime scxq2 --compile programs/main.json
echo    runtime scxq2 --list-ops
echo    runtime roundtrip programs/main.json
echo    runtime start
echo    runtime status
goto :eof
