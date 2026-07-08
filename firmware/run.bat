@echo off
echo ===================================
echo 1. Building firmware...
echo ===================================
make
if %errorlevel% neq 0 (
  echo.
  echo [ERROR] Build failed!
  pause
  exit /b %errorlevel%
)

echo.
echo ===================================
echo 2. Flashing firmware...
echo ===================================
pyocd erase -t py32f003x8 --chip --config ./Misc/pyocd.yaml
pyocd load Build/app.elf -t py32f003x8 --config ./Misc/pyocd.yaml
if %errorlevel% neq 0 (
  echo.
  echo [ERROR] Flashing failed!
  pause
  exit /b %errorlevel%
)

echo.
echo ===================================
echo 3. Starting RTT Monitor...
echo ===================================
pyocd rtt -t py32f003x8
pause
