#!/bin/bash
RED='\033[0;31m'
GRN='\033[0;32m'
NC='\033[0m'

check() {
    local name=$1
    local cmd=$2
    if command -v $cmd &> /dev/null; then
        echo -e "${GRN}[OK]${NC}  $name"
    else
        echo -e "${RED}[MISSING]${NC} $name"
    fi
}

echo "=== MegKer Toolchain Check ==="
echo ""
echo "--- Cross Compilers ---"
check "x86_64 GCC"   x86_64-elf-gcc
check "AArch64 GCC"  aarch64-elf-gcc
check "ARM32 GCC"    arm-none-eabi-gcc
echo ""
echo "--- Assemblers ---"
check "NASM"         nasm
echo ""
echo "--- Emulators ---"
check "QEMU x86_64"  qemu-system-x86_64
check "QEMU aarch64" qemu-system-aarch64
check "QEMU arm"     qemu-system-arm
echo ""
echo "--- Build Tools ---"
check "Git"          git
check "Make"         make
echo ""
echo "=== Done ==="