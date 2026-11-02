@echo off
rem arby-strata: serve Swift-Qwen3.8-Flash-Next from the IQ3_XXS pack, engine + C++ front end, ROCm 10.
rem
rem   run-arby-strata.bat            start on 0.0.0.0:8080
rem   run-arby-strata.bat 8081       start on another port
rem
rem The engine loads once: ~72 GB of GGUF across two shards plus the pack, an expert cache sized to the card and
rem a 131768-token context. One instance at a time - a second one on this box is what locked the machine up before,
rem so the port check below is not decorative.
rem
rem Console UI: if the built front end understands --ui (the live "Current Task" block), this launcher
rem turns it on and writes its log to arby-strata-ui.log. A front end that has never heard of the flag is
rem launched exactly as before - the check is the front end's own usage text, so no build flag needs
rem maintaining here. Force a mode by setting ARBY_UI before starting:
rem
rem   set ARBY_UI=live     the pinned block
rem   set ARBY_UI=plain    one line per state change, no ANSI
rem   set ARBY_UI=off      today's output, no block
setlocal EnableDelayedExpansion
for %%I in ("%~dp0.") do set "SRC=%%~fI"
set "PORT=%~1"
if not defined PORT set "PORT=8080"

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

rem ---- the numbers this box's card and this ROCm actually want ----
rem hipBLASLt in ROCm 10.2 is 1.5.0, so the table is the -100500 one. With the wrong table the engine silently
rem prefills on plain hipBLAS, about 45% slower at long context.
set "STRATA_HIPBLASLT_TUNING=%SRC%\tools\hip\gfx1201-hipblaslt-100500.txt"
set "STRATA_HIP_WMMA=1"
set "STRATA_PREFILL_RING=96"
set "STRATA_HELPER_CPUS=auto"

rem ---- console UI capability: does THIS front end know --ui? ----
rem Running it with no arguments prints its usage and exits 2 - no model, no engine, no card is touched.
rem It needs the ROCm bin directory on PATH, which is why this sits below. An older front end has no --ui
rem in that text and is then launched with no new flags at all.
set "UI_ARGS="
set "UI_MODE=off"
set "UILOG=%SRC%\arby-strata-ui.log"
if defined ARBY_UI goto :ui_mode_chosen
set "PROBE=%TEMP%\arby-strata-usage.txt"
"%FRONTEND%" >nul 2> "%PROBE%"
findstr /C:"--ui" "%PROBE%" >nul 2>&1
if errorlevel 1 goto :ui_probe_absent
del "%PROBE%" >nul 2>&1
set "ARBY_UI=live"
goto :ui_mode_chosen
:ui_probe_absent
del "%PROBE%" >nul 2>&1
goto :ui_ready
:ui_mode_chosen
if /I "%ARBY_UI%"=="off" goto :ui_ready
rem the log only ever grows: roll it over at 5 MB, keeping one previous copy
if exist "%UILOG%" for %%F in ("%UILOG%") do if %%~zF GTR 5242880 move /Y "%UILOG%" "%UILOG%.1" >nul
set UI_ARGS=--ui=%ARBY_UI% --log-file "%UILOG%"
set "UI_MODE=%ARBY_UI%"
:ui_ready

set "MDL=D:\Strata-data\models\swift-abliterated-iq3_xxs"
set "GGUF=%MDL%\IQ3_XXS\Swift-Qwen3.8-Flash-Next-GSQ-RCO-abliterated-IQ3_XXS-00001-of-00002.gguf"
set "MMPROJ=%MDL%\mmproj-Qwen3.8-Flash-Next-BF16.gguf"
set "PACK=D:\Strata-data\packs\swift-abliterated-iq3_xxs"
for %%F in ("%GGUF%") do if not exist "%%~fF" (echo Model not found: %%~fF & exit /b 1)

echo arby-strata :%PORT%  ^|  model %GGUF%  ^|  ui %UI_MODE%
echo.

"%FRONTEND%" --engine "%ENGINE%" -m "%GGUF%" --mmproj "%MMPROJ%" --host 0.0.0.0 --port %PORT% ^
  --alias swift-abliterated-iq3_xxs --threads 8 %UI_ARGS% ^
  -- --pack "%PACK%" ^
     --native "%GGUF%" ^
     --ple-gguf "%GGUF%" ^
     --vision ^
     --expert-profile "%SRC%\data\expert-profile.bin" ^
     --expert-cache auto --prefill auto ^
     --spec 4 --spec-min-p 0.5 ^
     --mtp D:\Strata-data\mtp\rt ^
     --max-context 131768 --kv int8 --kv-resident 32768 ^
     --vram-reserve-mib 1400

if errorlevel 1 (echo. & echo arby-strata exited with code %errorlevel% & pause)
endlocal
