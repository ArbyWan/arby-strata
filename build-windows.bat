@echo off
rem arby-strata on Windows: the HIP engine + the C++ front end, built against ROCm 10 from AMD's TheRock wheels.
rem
rem   build-windows.bat              configure and build build-hip-win/bin/{strata.exe, arby-strata.exe}
rem   build-windows.bat configure    configure only (fast: catches a toolchain problem without compiling)
rem
rem Needs: Visual Studio 2022/2026 Build Tools with the C++ workload (linker, CRT, Windows SDK), Python 3.10+
rem and git. No admin rights and no AMD GPU: ROCm comes from the pip wheels, installed into a private venv.
rem
rem Settings (environment variables, all optional):
rem   STRATA_ROCM_VERSION   10.2.0a20260930   the TheRock build (10.2 = the one upstream measured on an RX 9070 XT)
rem   STRATA_ROCM_INDEX     https://nightly.repo.amd.com/rocm/whl-next/
rem   STRATA_HIP_ARCHS      gfx1201           our card; "gfx1100;gfx1201" builds a binary for both
rem   ROCM_VENV             <repo>\.rocm-win
rem   BUILD_DIR             <repo>\build-hip-win
rem   PYTHON                python 3.12       the interpreter the venv is made from
setlocal EnableDelayedExpansion
for %%I in ("%~dp0.") do set "SRC=%%~fI"
if not defined STRATA_ROCM_VERSION set "STRATA_ROCM_VERSION=10.2.0a20260930"
if not defined STRATA_ROCM_INDEX set "STRATA_ROCM_INDEX=https://nightly.repo.amd.com/rocm/whl-next/"
if not defined STRATA_HIP_ARCHS set "STRATA_HIP_ARCHS=gfx1201"
if not defined ROCM_VENV set "ROCM_VENV=%SRC%\.rocm-win"
if not defined BUILD_DIR set "BUILD_DIR=%SRC%\build-hip-win"
if not defined PYTHON set "PYTHON=py -3.12"

rem ---- 1. ROCm: the compiler, the HIP runtime, hipBLAS/hipBLASLt/rocBLAS, and a device package per arch ----
if not exist "%ROCM_VENV%\Scripts\rocm-sdk.exe" (
  echo Creating the ROCm venv in "%ROCM_VENV%" ...
  %PYTHON% -m venv "%ROCM_VENV%" || (echo Python 3.10+ is needed ^(set PYTHON=^) & exit /b 1)
  set "EXTRAS=libraries,devel"
  for %%A in (%STRATA_HIP_ARCHS:;= %) do set "EXTRAS=!EXTRAS!,device-%%A"
  echo Installing ROCm %STRATA_ROCM_VERSION% [!EXTRAS!] ...
  "%ROCM_VENV%\Scripts\python.exe" -m pip install --disable-pip-version-check --index-url "%STRATA_ROCM_INDEX%" "rocm[!EXTRAS!]==%STRATA_ROCM_VERSION%" || exit /b 1
  "%ROCM_VENV%\Scripts\rocm-sdk.exe" init || exit /b 1
)
for /f "delims=" %%R in ('"%ROCM_VENV%\Scripts\rocm-sdk.exe" path --root') do set "ROCM=%%R"
if not exist "%ROCM%\lib\llvm\bin\clang++.exe" (echo ROCm has no compiler in "%ROCM%" & exit /b 1)
set "ROCM_F=%ROCM:\=/%"
set "BITCODE=%ROCM_F%/lib/llvm/amdgcn/bitcode"
set "HIP_PLATFORM=amd"
set "HIP_PATH=%ROCM%"
set "ROCM_PATH=%ROCM%"
set "PATH=%ROCM%\bin;%ROCM%\lib\llvm\bin;%PATH%"
echo ROCm: %ROCM%

rem ---- 2. MSVC: the linker, the CRT and the Windows SDK (ROCm's clang targets the MSVC ABI) ----
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS="
if exist "%VSWHERE%" for /f "delims=" %%V in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS=%%V"
if not defined VS (echo Visual Studio Build Tools with the C++ workload are needed & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

rem CMake and Ninja: whatever is on PATH, else the copies Visual Studio installs
where cmake >nul 2>nul || set "PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where cmake >nul 2>nul || (echo CMake is needed ^(install it or the VS C++ workload^) & exit /b 1)
where ninja >nul 2>nul || set "PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

rem ---- 3. configure: engine (HIP) + front end (no GPU) in one build tree ----
rem CMake refuses to mix cl.exe with Clang for HIP, so ROCm's clang compiles the host code too, against MSVC's
rem headers and linker (the pattern upstream's tools\hip\build_windows.bat uses).
if not exist "%BUILD_DIR%\build.ninja" (
  cmake -G Ninja -S "%SRC%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release ^
    -DSTRATA_ENABLE_HIP=ON -DSTRATA_ENABLE_CUDA=OFF -DSTRATA_BUILD_TESTS=OFF -DSTRATA_PREFILL_MMQ=ON ^
    -DSTRATA_NATIVE_EXPERTS=ON -DSTRATA_FRONTEND=ON "-DCMAKE_HIP_ARCHITECTURES=%STRATA_HIP_ARCHS%" ^
    "-DCMAKE_C_COMPILER=%ROCM_F%/lib/llvm/bin/clang.exe" "-DCMAKE_CXX_COMPILER=%ROCM_F%/lib/llvm/bin/clang++.exe" ^
    "-DCMAKE_HIP_COMPILER=%ROCM_F%/lib/llvm/bin/clang++.exe" "-DCMAKE_HIP_COMPILER_ROCM_ROOT=%ROCM_F%" ^
    "-DCMAKE_PREFIX_PATH=%ROCM_F%" "-DCMAKE_HIP_FLAGS=--rocm-path=%ROCM_F% --rocm-device-lib-path=%BITCODE%" ^
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=%BUILD_DIR:\=/%/bin" || exit /b 1
)
if /i "%~1"=="configure" (echo Configured %BUILD_DIR% & endlocal & exit /b 0)

rem ---- 4. build: the engine and the front end ----
cmake --build "%BUILD_DIR%" --target strata arby-strata || exit /b 1
echo.
echo Built:
dir /b "%BUILD_DIR%\bin\strata.exe" "%BUILD_DIR%\bin\arby-strata.exe" 2>nul
echo.
echo Run (export the tuning table that matches hipBLASLt 1.5.0 in this ROCm):
echo   set STRATA_HIPBLASLT_TUNING=%SRC%\tools\hip\gfx1201-hipblaslt-100500.txt
echo   "%BUILD_DIR%\bin\arby-strata.exe" --engine strata -m ^<model.gguf^> --host 127.0.0.1 --port 8080 -- ^<engine args^>
endlocal
