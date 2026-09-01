#!/bin/bash
set -e

# ==============================================================================
# Vailism OS - macOS Development Environment Setup Script
# Works on both Apple Silicon (M1/M2/M3/M4) and Intel Macs
# ==============================================================================

echo "=================================================="
echo "   Vailism OS - macOS Host Setup & Prerequisites"
echo "=================================================="

# 1. Check for Homebrew
if ! command -v brew &> /dev/null; then
    echo "[!] Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    
    # Configure PATH for Apple Silicon / Intel
    if [ -f /opt/homebrew/bin/brew ]; then
        eval "$(/opt/homebrew/bin/brew shellenv)"
    elif [ -f /usr/local/bin/brew ]; then
        eval "$(/usr/local/bin/brew shellenv)"
    fi
else
    echo "[✓] Homebrew is installed."
fi

# 2. Install Required Tools via Homebrew
echo "[*] Installing required build tools (LLVM/Clang, LLD, NASM, QEMU, Xorriso)..."
brew install llvm nasm xorriso qemu

# 3. Verify Toolchain
echo ""
echo "=== Toolchain Verification ==="
LLVM_PATH=$(brew --prefix llvm)
echo "[✓] LLVM Clang:  ${LLVM_PATH}/bin/clang ($(${LLVM_PATH}/bin/clang --version | head -n 1))"
echo "[✓] LLVM LLD:    ${LLVM_PATH}/bin/ld.lld ($(${LLVM_PATH}/bin/ld.lld --version | head -n 1))"
echo "[✓] NASM:        $(which nasm) ($(nasm -v))"
echo "[✓] Xorriso:     $(which xorriso) ($(xorriso --version 2>&1 | head -n 1))"
echo "[✓] QEMU x86_64: $(which qemu-system-x86_64) ($(qemu-system-x86_64 --version | head -n 1))"

echo ""
echo "=================================================="
echo "   macOS Setup Complete! You're ready to build."
echo "   Run 'make' to compile or 'make run' to test."
echo "=================================================="
