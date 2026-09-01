#!/bin/bash
set -e

# ==============================================================================
# Vailism OS - macOS Launch Script
# ==============================================================================

# Determine Host Architecture
ARCH=$(uname -m)
echo "[*] Host: macOS ($ARCH)"

# Build latest ISO
make iso

# Run QEMU with optimal macOS Cocoa settings
echo "[*] Launching Vailism OS in QEMU (Cocoa frontend)..."
echo "    - Click inside the window to capture the trackpad/mouse."
echo "    - Press Ctrl+Alt+G at any time to release the mouse."
echo "    - Press Ctrl+C in terminal or close window to exit."

make run
