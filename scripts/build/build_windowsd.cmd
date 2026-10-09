@echo off
setlocal
rem Run in the MSVC developer shell; SDK paths belong in CMakeUserPresets.json.
for %%I in ("%~dp0..\..") do set "QUALPF_REPO_DIR=%%~fI"
if not defined CMAKE_COMMAND set "CMAKE_COMMAND=cmake"
pushd "%QUALPF_REPO_DIR%"
"%CMAKE_COMMAND%" --preset windows-msvc -B "%QUALPF_REPO_DIR%/build/windows-msvc-debug" -DCMAKE_BUILD_TYPE=DEBUG %*
set "QUALPF_EXIT_CODE=%errorlevel%"
popd
exit /b %QUALPF_EXIT_CODE%
