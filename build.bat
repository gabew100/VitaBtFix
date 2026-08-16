@echo off
if "%VITASDK%"=="" (
  echo Set VITASDK to your VitaSDK path, e.g. set VITASDK=C:\vitasdk
  exit /b 1
)
if not exist build mkdir build
cd build
cmake .. -G "Unix Makefiles"
if errorlevel 1 cmake ..
make