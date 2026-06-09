@echo off
REM Build a "standalone" HH.exe that statically links SFML, so that the
REM only DLL needed at runtime is openal32.dll (LGPL -- has to remain a
REM DLL). Run _package_standalone.ps1 afterwards to bundle the result
REM with openal32.dll and the resources/ folder into a sendable zip.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >NUL

if not exist build-standalone (
    cmake -S . -B build-standalone -G Ninja ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DHH_STATIC_SFML=ON ^
        -DBUILD_TESTING=OFF
    if errorlevel 1 exit /b 1
)
cmake --build build-standalone --target HH
