@echo off
rem Baut Clipline (Release) und erzeugt dist\Clipline-windows-x64.zip (portabel, mit Qt + ffmpeg).
setlocal
set QTDIR=C:\Qt\6.8.3\msvc2022_64
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
set VSCM=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake
set CMAKE="%VSCM%\CMake\bin\cmake.exe"
set PATH=%VSCM%\Ninja;%PATH%
%CMAKE% -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QTDIR% || exit /b 1
%CMAKE% --build build || exit /b 1
if not exist build\ffmpeg.exe (
  for /f "delims=" %%F in ('python -c "import imageio_ffmpeg as i;print(i.get_ffmpeg_exe())"') do copy /y "%%F" build\ffmpeg.exe >nul
)
"%QTDIR%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw build\Clipline.exe >nul
if exist dist\Clipline rmdir /s /q dist\Clipline
mkdir dist\Clipline
"%QTDIR%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw --dir dist\Clipline build\Clipline.exe >nul
copy /y build\Clipline.exe dist\Clipline\ >nul
copy /y build\ffmpeg.exe dist\Clipline\ >nul
copy /y LICENSE dist\Clipline\ >nul 2>nul
copy /y README.md dist\Clipline\ >nul 2>nul
powershell -NoProfile -Command "Compress-Archive -Path dist\Clipline -DestinationPath dist\Clipline-windows-x64.zip -Force"
echo Fertig: build\Clipline.exe und dist\Clipline-windows-x64.zip
