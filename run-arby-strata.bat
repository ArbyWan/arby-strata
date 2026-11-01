@echo off
rem arby-strata: serve Swift-Qwen3.8-Flash-Next from the IQ3_XXS pack, engine + C++ front end, ROCm 10.
rem
rem   run-arby-strata.bat            start on 0.0.0.0:8080
rem   run-arby-strata.bat 8081       start on another port
rem
rem The engine loads once: ~72 GB of GGUF across two shards plus the pack, an expert cache sized to the card and
rem a 131768-token context. One instance at a time - a second one on this box is what locked the machine up before,
rem so the port check below is not decorative.
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

set "MDL=D:\Strata-data\models\swift-abliterated-iq3_xxs"
set "GGUF=%MDL%\IQ3_XXS\Swift-Qwen3.8-Flash-Next-GSQ-RCO-abliterated-IQ3_XXS-00001-of-00002.gguf"
set "MMPROJ=%MDL%\mmproj-Qwen3.8-Flash-Next-BF16.gguf"
set "PACK=D:\Strata-data\packs\swift-abliterated-iq3_xxs"
for %%F in ("%GGUF%") do if not exist "%%~fF" (echo Model not found: %%~fF & exit /b 1)

echo arby-strata on port %PORT%
echo   engine  %ENGINE%
echo   front   %FRONTEND%
echo   model   %GGUF%
echo   pack    %PACK%
echo   rocm    %ROCM%
echo.

"%FRONTEND%" --engine "%ENGINE%" -m "%GGUF%" --mmproj "%MMPROJ%" --host 0.0.0.0 --port %PORT% ^
  --alias swift-abliterated-iq3_xxs --threads 8 ^
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
