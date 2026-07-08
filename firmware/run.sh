#!/bin/bash
set -e

echo "==================================="
echo "1. Building firmware..."
echo "==================================="
make

echo ""
echo "==================================="
echo "2. Flashing firmware..."
echo "==================================="
pyocd erase -t py32f003x8 --chip --config ./Misc/pyocd.yaml
pyocd load Build/app.elf -t py32f003x8 --config ./Misc/pyocd.yaml

echo ""
echo "==================================="
echo "3. Starting RTT Monitor..."
echo "==================================="
pyocd rtt -t py32f003x8
