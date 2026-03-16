#!/usr/bin/env bash
set -euo pipefail

echo "==> Checking for Homebrew..."
if ! command -v brew &>/dev/null; then
  echo "    Installing Homebrew..."
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
  echo "    Homebrew found: $(brew --version | head -1)"
fi

echo "==> Checking for arduino-cli..."
if ! command -v arduino-cli &>/dev/null; then
  echo "    Installing arduino-cli..."
  brew install arduino-cli
else
  echo "    arduino-cli found: $(arduino-cli version)"
fi

echo "==> Adding ESP32 board index..."
arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json

echo "==> Updating board index..."
arduino-cli core update-index

echo "==> Installing ESP32 core (3.3.7)..."
arduino-cli core install esp32:esp32@3.3.7

echo ""
echo "Setup complete. Run ./build.sh to compile the firmware."
