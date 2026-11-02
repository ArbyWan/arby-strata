@echo off
rem arby-strata live console UI — build and run the state machine on its own.
rem No GPU, no model, no llama.cpp, no repo build tree: two small C++ files and a test.
rem
rem   build_and_test.bat              build, run, keep the window open (double-click safe)
rem   build_and_test.bat --no-pause   same, but return at once (scripts / CI)
rem
rem Everything is captured to live_task_test.log next to this script, so a window that closes can
rem never hide the result again.
setlocal EnableDelayedExpansion
set "SRC=%~dp0"
set "LOG=%SRC%live_task_test.log"
set "NOPAUSE="
if /I "%~1"=="--no-pause" set "NOPAUSE=1"

call :run > "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"
type "%LOG%"

echo.
if "%RC%"=="0" (
  echo RESULT: PASS   ^(full output in live_task_test.log^)
) else (
  echo RESULT: FAIL   exit %RC%   ^(full output in live_task_test.log^)
)
if not defined NOPAUSE pause
exit /b %RC%

rem ---------------------------------------------------------------------------------------------
:run
set "CXX="

rem 1) the ROCm 10 venv build-windows.bat uses: repo-root relative first, then the known absolute
rem    location, then an explicit ROCM_ROOT override.
for %%C in (
  "%SRC%..\.rocm-win\Scripts\rocm-sdk.exe"
  "D:\arby-strata\.rocm-win\Scripts\rocm-sdk.exe"
) do (
  if not defined ROCM if exist %%C for /f "delims=" %%R in ('%%C path --root') do set "ROCM=%%R"
)
if defined ROCM_ROOT set "ROCM=%ROCM_ROOT%"
if defined ROCM if exist "%ROCM%\lib\llvm\bin\clang++.exe" set "CXX=%ROCM%\lib\llvm\bin\clang++.exe"
if not defined CXX (
  echo No clang++.exe found. Looked for the ROCm venv at:
  echo   "%SRC%..\.rocm-win\Scripts\rocm-sdk.exe"
  echo   "D:\arby-strata\.rocm-win\Scripts\rocm-sdk.exe"
  echo   %%ROCM_ROOT%%\lib\llvm\bin\clang++.exe
  exit /b 2
)

rem 2) MSVC supplies the CRT, the linker and the Windows SDK (ROCm's clang targets the MSVC ABI).
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS="
if exist "%VSWHERE%" for /f "delims=" %%V in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS=%%V"
if not defined VS (
  echo No Visual Studio C++ tools found ^(vswhere reported nothing^).
  exit /b 3
)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
  echo vcvars64.bat failed under "%VS%".
  exit /b 3
)

echo clang++ : %CXX%
echo sources : %SRC%
echo.

echo --- compile + link the state machine and its test ---
"%CXX%" -std=c++20 -O1 -Wall -Wextra -Wno-unused-parameter -D_CRT_SECURE_NO_WARNINGS -I"%SRC%." ^
    "%SRC%live_task.cpp" "%SRC%live_task_test.cpp" -o "%SRC%live_task_test.exe"
if errorlevel 1 (
  echo.
  echo COMPILE FAILED for live_task.cpp / live_task_test.cpp ^(see the errors above^).
  exit /b 1
)

echo --- syntax check of the terminal layer ^(compiled, not linked^) ---
"%CXX%" -std=c++20 -Wall -Wextra -Wno-unused-parameter -D_CRT_SECURE_NO_WARNINGS -I"%SRC%." ^
    -c "%SRC%ui_live.cpp" -o "%SRC%ui_live.o"
if errorlevel 1 (
  echo.
  echo COMPILE FAILED for ui_live.cpp.
  exit /b 1
)
echo ui_live.cpp compiles clean.
echo.

echo --- run the tests ---
"%SRC%live_task_test.exe"
if errorlevel 1 (
  echo.
  echo TESTS FAILED.
  exit /b 1
)
exit /b 0
