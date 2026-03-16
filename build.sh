#!/usr/bin/env bash
set -euo pipefail

SKETCH="ats-mini"
PROFILE="${1:-esp32s3-ospi}"   # default to ospi; pass 'esp32s3-qspi' for quad-SPI variant

if [[ "$PROFILE" != "esp32s3-ospi" && "$PROFILE" != "esp32s3-qspi" ]]; then
  echo "Usage: $0 [esp32s3-ospi|esp32s3-qspi]"
  exit 1
fi

echo "==> Building profile: $PROFILE"
arduino-cli compile \
  --profile "$PROFILE" \
  --export-binaries \
  "$SKETCH"

echo ""
echo "Build complete. Binaries are in $SKETCH/build/"
echo ""
echo "To flash (replace /dev/cu.usbmodem* with your device):"
echo "  esptool.py --chip esp32s3 --port /dev/cu.usbmodem* write_flash 0x0 \\"
echo "    $SKETCH/build/esp32.esp32.esp32s3/*.ino.bootloader.bin"
