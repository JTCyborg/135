@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Project135 - Sync, Build and Test

set "ROOT=%~dp0"
cd /d "%ROOT%"
if errorlevel 1 (
    echo ERROR: Cannot enter project directory: "%ROOT%"
    exit /b 10
)

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%i"
if not defined STAMP set "STAMP=%RANDOM%_%RANDOM%"
if not exist "BuildReports" mkdir "BuildReports"
set "LOG=BuildReports\Project135_validation_%STAMP%.log"

set "OVERALL=PASS"
set "DEBUG_BUILD_OK=1"
set "RELEASE_BUILD_OK=1"
set "DEBUG_TEST_OK=SKIPPED"
set "RELEASE_TEST_OK=SKIPPED"
set "PUBLISH_OK=NOT_ATTEMPTED"

echo ================================================================ > "%LOG%"
echo Project135 validation run >> "%LOG%"
echo Started: %DATE% %TIME% >> "%LOG%"
echo Project: %ROOT% >> "%LOG%"
echo ================================================================ >> "%LOG%"

echo.
echo Project135 validation
echo Log: %LOG%
echo.

where git >nul 2>&1
if errorlevel 1 (
    echo ERROR: git was not found in PATH.
    echo [FATAL] git was not found in PATH.>>"%LOG%"
    set "OVERALL=FAILED"
    goto STOP_WITH_LOCAL_LOG
)
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmake was not found in PATH.
    echo [FATAL] cmake was not found in PATH.>>"%LOG%"
    set "OVERALL=FAILED"
    goto STOP_WITH_LOCAL_LOG
)

echo [ENVIRONMENT]>>"%LOG%"
echo Computer: %COMPUTERNAME%>>"%LOG%"
echo User: %USERNAME%>>"%LOG%"
echo.>>"%LOG%"
set "RUN_CMD=cmake --version"
call :RUN "CMake version"
set "RUN_CMD=git --version"
call :RUN "Git version"

rem Remove only generated output folders requested by the user.
echo.
echo [1/6] Cleaning CAD, Drawings and Debug...
for %%D in (CAD Drawings Debug) do (
    if exist "%%D" (
        echo Removing %%D...
        rd /s /q "%%D"
        if exist "%%D" (
            echo ERROR: Could not remove %%D.
            echo [FATAL] Could not remove %%D.>>"%LOG%"
            set "OVERALL=FAILED"
            goto STOP_WITH_LOCAL_LOG
        )
    )
)
echo [CLEAN] CAD, Drawings and Debug removed.>>"%LOG%"
mkdir "CAD" 2>nul
mkdir "Drawings" 2>nul
mkdir "Debug" 2>nul

rem Never overwrite local source changes with a pull.
echo.
echo [2/6] Checking Git state and synchronizing main...
set "CURRENT_BRANCH="
for /f %%B in ('git branch --show-current 2^>nul') do set "CURRENT_BRANCH=%%B"
echo Current branch: %CURRENT_BRANCH%>>"%LOG%"
if not "%CURRENT_BRANCH%"=="main" (
    echo ERROR: Current branch is "%CURRENT_BRANCH%", expected "main".
    echo [FATAL] Expected branch main; found %CURRENT_BRANCH%.>>"%LOG%"
    set "OVERALL=FAILED"
    goto STOP_WITH_LOCAL_LOG
)
set "DIRTY="
for /f "delims=" %%S in ('git status --porcelain --untracked-files^=all') do set "DIRTY=1"
if defined DIRTY (
    echo ERROR: Git working tree is not clean. Commit or stash source changes, then rerun.
    echo [FATAL] Working tree is not clean after generated-folder cleanup. Pull was not run.>>"%LOG%"
    git status --short >>"%LOG%" 2>&1
    set "OVERALL=FAILED"
    goto STOP_WITH_LOCAL_LOG
)

set "RUN_CMD=git fetch origin"
call :RUN "Git fetch origin"
if errorlevel 1 (
    set "OVERALL=FAILED"
    goto STOP_WITH_LOCAL_LOG
)
set "RUN_CMD=git pull --ff-only origin main"
call :RUN "Git pull --ff-only origin main"
if errorlevel 1 (
    set "OVERALL=FAILED"
    goto STOP_WITH_LOCAL_LOG
)
for /f %%H in ('git rev-parse HEAD') do set "SOURCE_COMMIT=%%H"
echo Synchronized source commit: %SOURCE_COMMIT%>>"%LOG%"

rem Configure both CMake presets before building.
echo.
echo [3/6] Configuring Debug and Release...
set "RUN_CMD=cmake --preset vs2026-desktop-debug"
call :RUN "Configure Debug"
if errorlevel 1 (
    set "OVERALL=FAILED"
    goto FINALIZE
)
set "RUN_CMD=cmake --preset vs2026-desktop-release"
call :RUN "Configure Release"
if errorlevel 1 (
    set "OVERALL=FAILED"
    goto FINALIZE
)

echo.
echo [4/6] Building Debug and Release...
set "RUN_CMD=cmake --build --preset build-desktop-debug --parallel"
call :RUN "Build Debug"
if errorlevel 1 (
    set "DEBUG_BUILD_OK=0"
    set "OVERALL=FAILED"
)
set "RUN_CMD=cmake --build --preset build-desktop-release --parallel"
call :RUN "Build Release"
if errorlevel 1 (
    set "RELEASE_BUILD_OK=0"
    set "OVERALL=FAILED"
)

echo.
echo [5/6] Running CTest...
if "%DEBUG_BUILD_OK%"=="1" (
    set "RUN_CMD=ctest --preset test-desktop-debug"
    call :RUN "CTest Debug"
    if errorlevel 1 (
        set "DEBUG_TEST_OK=FAIL"
        set "OVERALL=FAILED"
    ) else (
        set "DEBUG_TEST_OK=PASS"
    )
) else (
    echo [SKIP] Debug tests skipped because Debug build failed.>>"%LOG%"
)
if "%RELEASE_BUILD_OK%"=="1" (
    set "RUN_CMD=ctest --preset test-desktop-release"
    call :RUN "CTest Release"
    if errorlevel 1 (
        set "RELEASE_TEST_OK=FAIL"
        set "OVERALL=FAILED"
    ) else (
        set "RELEASE_TEST_OK=PASS"
    )
) else (
    echo [SKIP] Release tests skipped because Release build failed.>>"%LOG%"
)

:FINALIZE
echo.>>"%LOG%"
echo ===================== FINAL SUMMARY =====================>>"%LOG%"
echo Overall: %OVERALL%>>"%LOG%"
echo Debug build: %DEBUG_BUILD_OK%>>"%LOG%"
echo Debug tests: %DEBUG_TEST_OK%>>"%LOG%"
echo Release build: %RELEASE_BUILD_OK%>>"%LOG%"
echo Release tests: %RELEASE_TEST_OK%>>"%LOG%"
echo Source commit: %SOURCE_COMMIT%>>"%LOG%"
echo Finished: %DATE% %TIME%>>"%LOG%"
echo ==========================================================>>"%LOG%"

rem Publish the report even if a build or test failed.
echo.
echo [6/6] Publishing validation report to GitHub...
git add -- "%LOG%" >>"%LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: Could not stage the report.
    set "PUBLISH_OK=STAGE_FAILED"
    goto END_LOCAL
)
git commit -m "Add Project135 validation report %STAMP%" >>"%LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: Could not commit the report.
    set "PUBLISH_OK=COMMIT_FAILED"
    goto END_LOCAL
)
git push origin main >>"%LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: Git push failed. The report commit is local only.
    set "PUBLISH_OK=PUSH_FAILED"
    goto END_LOCAL
)
set "PUBLISH_OK=PUSHED"
echo [GIT_PUSH] SUCCESS - validation report was pushed to origin/main.>>"%LOG%"
rem Store the push result in Git too (second, report-only commit).
git add -- "%LOG%" >>"%LOG%" 2>&1
if errorlevel 1 (
    set "PUBLISH_OK=PUSHED_REPORT_STATUS_NOT_COMMITTED"
    goto END_LOCAL
)
git commit -m "Record Project135 report synchronization %STAMP%" >>"%LOG%" 2>&1
if errorlevel 1 (
    set "PUBLISH_OK=PUSHED_REPORT_STATUS_NOT_COMMITTED"
    goto END_LOCAL
)
git push origin main >>"%LOG%" 2>&1
if errorlevel 1 (
    set "PUBLISH_OK=FIRST_PUSHED_SECOND_PUSH_FAILED"
    goto END_LOCAL
)

:END_LOCAL
echo.
echo ================================================================
echo Overall: %OVERALL%
echo Debug build: %DEBUG_BUILD_OK%  Debug tests: %DEBUG_TEST_OK%
echo Release build: %RELEASE_BUILD_OK%  Release tests: %RELEASE_TEST_OK%
echo Report publication: %PUBLISH_OK%
echo Log: %ROOT%%LOG%
echo ================================================================
echo.
if not "%PUBLISH_OK%"=="PUSHED" (
    echo WARNING: The report may need manual Git synchronization. See the log.
)
pause
if "%OVERALL%"=="PASS" if "%PUBLISH_OK%"=="PUSHED" exit /b 0
exit /b 1

:STOP_WITH_LOCAL_LOG
echo.>>"%LOG%"
echo Overall: %OVERALL%>>"%LOG%"
echo Synchronization/build stopped before CMake configuration.>>"%LOG%"
echo Finished: %DATE% %TIME%>>"%LOG%"
echo.
echo STOPPED. No source changes were overwritten. Local log: %ROOT%%LOG%
pause
exit /b 1

:RUN
echo [RUN] %~1
echo.>>"%LOG%"
echo [RUN] %~1>>"%LOG%"
call %RUN_CMD% >>"%LOG%" 2>&1
set "RUN_RC=%ERRORLEVEL%"
if "%RUN_RC%"=="0" (
    echo [PASS] %~1
    echo [PASS] %~1>>"%LOG%"
) else (
    echo [FAIL] %~1 (exit %RUN_RC%)
    echo [FAIL] %~1 (exit %RUN_RC%)>>"%LOG%"
)
exit /b %RUN_RC%
