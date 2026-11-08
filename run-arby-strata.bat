@echo off
rem arby-strata: serve a GGUF model from its IQ3_XXS pack, engine + C++ front end, ROCm 10.
rem
rem   run-arby-strata.bat            start on 0.0.0.0:8080
rem   run-arby-strata.bat 8081       start on another port
rem
rem The engine loads once: the GGUF shards plus the pack, an expert cache sized to the card and the context
rem given below. One instance at a time - a second one on the same box has locked the machine up before, so
rem the port check below is not decorative.
rem
rem Console UI: off by default. If the built front end understands --ui (the live "Current Task" block),
rem setting ARBY_UI turns it on and writes its log to arby-strata-ui.log. A front end that has never heard
rem of the flag is launched exactly as before - the check is the front end's own usage text, so no build
rem flag needs maintaining here. Choose a mode by setting ARBY_UI before starting:
rem
rem   set ARBY_UI=live     the pinned block
rem   set ARBY_UI=plain    one line per state change, no ANSI
rem   set ARBY_UI=off      today's output, no block
setlocal EnableDelayedExpansion
for %%I in ("%~dp0.") do set "SRC=%%~fI"
set "PORT=%~1"
if not defined PORT set "PORT=8080"

rem ---- optional: MTP verify window (--spec), default 4 -------------------------------------------
rem %2 lets a draft-acceptance A/B switch arms without a second launcher:
rem   run-arby-strata.bat 8080 4     (baseline)
rem   run-arby-strata.bat 8080 5
set "SPEC=%~2"
if not defined SPEC set "SPEC=4"
echo %SPEC%| findstr /R "^[1-8]$" >nul || (echo --spec must be 1..8 ^(got "%SPEC%"^) & exit /b 1)

set "ENGINE=%SRC%\build-hip-win\bin\strata.exe"
set "FRONTEND=%SRC%\build-hip-win\bin\arby-strata.exe"
if not exist "%FRONTEND%" (echo No front end at "%FRONTEND%" - run build-windows.bat first & exit /b 1)
if not exist "%ENGINE%"   (echo No engine   at "%ENGINE%"   - run build-windows.bat first & exit /b 1)

netstat -ano | findstr /R ":%PORT%[ >]" | findstr /I "LISTENING" >nul
if not errorlevel 1 (
  echo Port %PORT% is already in use - something is serving. Close it first ^(never two Strata instances^).
  exit /b 1
)

rem ---- ROCm 10 from the private venv: the engine loads rocblas.dll / hipblaslt.dll from here ----
for /f "delims=" %%R in ('"%SRC%\.rocm-win\Scripts\rocm-sdk.exe" path --root') do set "ROCM=%%R"
if not exist "%ROCM%\bin\rocblas.dll" (echo ROCm looks wrong at "%ROCM%" & exit /b 1)
set "HIP_PLATFORM=amd"
set "HIP_PATH=%ROCM%"
set "ROCM_PATH=%ROCM%"
set "PATH=%ROCM%\bin;%ROCM%\lib\llvm\bin;%PATH%"

rem ---- the numbers a gfx1201 card and this ROCm actually want ----
rem hipBLASLt in ROCm 10.2 is 1.5.0, so the table is the -100500 one. With the wrong table the engine silently
rem prefills on plain hipBLAS, about 45% slower at long context.
set "STRATA_HIPBLASLT_TUNING=%SRC%\tools\hip\gfx1201-hipblaslt-100500.txt"
set "STRATA_HIP_WMMA=1"
set "STRATA_PREFILL_RING=96"
set "STRATA_HELPER_CPUS=auto"

rem ---- console UI: opt-in, and only where THIS front end knows --ui ----
rem Nothing is passed unless ARBY_UI names a mode, so the default is the output of a build that never
rem heard of the flag. Running the front end with no arguments prints its usage and exits 2 - no model,
rem no engine, no card is touched - so an explicitly requested mode is checked against that text first.
rem It needs the ROCm bin directory on PATH, which is why this sits below. An older front end has no
rem --ui in that text and is then launched with no new flags at all.
set "UI_ARGS="
set "UI_MODE=off"
set "UILOG=%SRC%\arby-strata-ui.log"
if not defined ARBY_UI goto :ui_ready
set "PROBE=%TEMP%\arby-strata-usage.txt"
"%FRONTEND%" >nul 2> "%PROBE%"
findstr /C:"--ui" "%PROBE%" >nul 2>&1
if errorlevel 1 goto :ui_probe_absent
del "%PROBE%" >nul 2>&1
if /I "%ARBY_UI%"=="off" goto :ui_ready
rem the log only ever grows: roll it over at 5 MB, keeping one previous copy
if exist "%UILOG%" for %%F in ("%UILOG%") do if %%~zF GTR 5242880 move /Y "%UILOG%" "%UILOG%.1" >nul
set UI_ARGS=--ui=%ARBY_UI% --log-file "%UILOG%"
set "UI_MODE=%ARBY_UI%"
goto :ui_ready
:ui_probe_absent
del "%PROBE%" >nul 2>&1
echo [ui] this front end has no --ui - starting without it
:ui_ready

rem ---- the model: edit these four, or set them in the environment before starting ----
rem   MDL     the folder holding the GGUF shards and, if the model has one, the vision projector
rem   GGUF    the first shard (-00001-of-00002); the engine loads the rest beside it
rem   MMPROJ  the vision projector for --mmproj; leave --mmproj off below if the model has none
rem   PACK    the pack built for the same model (packs\<model>)
if not defined MDL set "MDL=%~dp0models\my-model"
if not defined GGUF set "GGUF=%MDL%\IQ3_XXS\my-model-IQ3_XXS-00001-of-00002.gguf"
if not defined MMPROJ set "MMPROJ=%MDL%\mmproj-my-model-BF16.gguf"
if not defined PACK set "PACK=%~dp0packs\my-model"
for %%F in ("%GGUF%") do if not exist "%%~fF" (echo Model not found: %%~fF & exit /b 1)

echo arby-strata :%PORT%  ^|  model %GGUF%  ^|  spec %SPEC%  ^|  ui %UI_MODE%
echo.

rem ---- the dashboard: opt-in, its own window ------------------------------------------------
rem The TUI is deliberately a SEPARATE process: an alt-screen renderer inside this one would share its
rem fate (a blocked console write freezes inference, which has bitten us before) and would stop the
rem server being headless. Set STRATA_TOP and it opens with the server; unset (the default) the server
rem starts on its own.
rem   set STRATA_TOP=<path>       the dashboard to open, e.g. strata-top\strata-top.bat - off when unset
rem   set ARBY_NO_TUI=1           never open it, even when STRATA_TOP is set
if not defined ARBY_NO_TUI if defined STRATA_TOP if exist "%STRATA_TOP%" (
  rem /status answers while the model is still loading, so the dashboard opens straight into "Loading"
  start "strata-top :%PORT%" "%STRATA_TOP%" 127.0.0.1:%PORT%
)

rem --mtp is the draft-model folder a speculative run reads; drop the flag if the model has none
"%FRONTEND%" --engine "%ENGINE%" -m "%GGUF%" --mmproj "%MMPROJ%" --host 0.0.0.0 --port %PORT% ^
  --alias my-model --threads 8 %UI_ARGS% ^
  -- --pack "%PACK%" ^
     --native "%GGUF%" ^
     --ple-gguf "%GGUF%" ^
     --vision ^
     --expert-profile "%SRC%\data\expert-profile.bin" ^
     --expert-cache auto --prefill auto:32768 ^
     --spec %SPEC% --spec-min-p 0.5 ^
     --mtp "%~dp0mtp\rt" ^
     --max-context 131768 --kv int8 --kv-resident 32768 ^
     --vram-reserve-mib 1400

if errorlevel 1 (echo. & echo arby-strata exited with code %errorlevel% & pause)
endlocal
