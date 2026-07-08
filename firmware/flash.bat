@echo off
echo Flashing firmware using pyOCD...
pyocd erase -t py32f003x8 --chip --config ./Misc/pyocd.yaml
pyocd load Build/app.elf -t py32f003x8 --config ./Misc/pyocd.yaml
echo Flashing complete!
pause
