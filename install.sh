#!/bin/bash
# HASH256 CUDA GPU Miner — auto installer
# Run inside Ubuntu WSL2: bash <(curl -s https://raw.githubusercontent.com/vozlemetro/hash256-miner/main/install.sh)

set -e

echo ""
echo "  _   _    _    ____  _   _ ____  ____   __"
echo " | | | |  / \  / ___|| | | |___ \| ___| / /_"
echo " | |_| | / _ \ \___ \| |_| | __) |___ \| '_ \\"
echo " |  _  |/ ___ \ ___) |  _  |/ __/ ___) | (_) |"
echo " |_| |_/_/   \_\____/|_| |_|_____|____/ \___/"
echo ""
echo "       CUDA GPU Miner — Installer"
echo ""

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log()  { echo -e "${GREEN}[+]${NC} $1"; }
warn() { echo -e "${YELLOW}[!]${NC} $1"; }
err()  { echo -e "${RED}[x]${NC} $1"; }

# Check we're on Linux/WSL
if [[ "$(uname -s)" != "Linux" ]]; then
    err "This script must be run inside WSL2 (Ubuntu)."
    err "Open PowerShell and run: wsl --install -d Ubuntu-22.04"
    exit 1
fi

# Check NVIDIA GPU is visible
if ! command -v nvidia-smi &>/dev/null; then
    err "nvidia-smi not found. Make sure you have NVIDIA drivers installed on Windows."
    err "Download from: https://www.nvidia.com/drivers"
    exit 1
fi

log "NVIDIA GPU detected:"
nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null || true
echo ""

# Step 1: Install CUDA Toolkit if missing
if command -v nvcc &>/dev/null; then
    log "CUDA already installed: $(nvcc --version 2>/dev/null | grep release | awk '{print $5}' | tr -d ',')"
else
    log "Installing CUDA Toolkit 12.6..."
    wget -q https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb -O /tmp/cuda-keyring.deb
    sudo dpkg -i /tmp/cuda-keyring.deb
    sudo apt-get update -qq
    sudo apt-get install -y -qq cuda-toolkit-12-6
    export PATH=/usr/local/cuda/bin:$PATH
    if ! grep -q 'cuda/bin' ~/.bashrc 2>/dev/null; then
        echo 'export PATH=/usr/local/cuda/bin:$PATH' >> ~/.bashrc
    fi
    log "CUDA installed: $(nvcc --version 2>/dev/null | grep release | awk '{print $5}' | tr -d ',')"
fi

# Step 2: Install build dependencies
log "Installing build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq cmake build-essential libcurl4-openssl-dev libsecp256k1-dev git

# Step 3: Clone or update repo
INSTALL_DIR="$HOME/hash256-miner"
if [ -d "$INSTALL_DIR/.git" ]; then
    log "Updating existing installation..."
    cd "$INSTALL_DIR"
    git pull --ff-only origin main 2>/dev/null || true
else
    log "Cloning hash256-miner..."
    rm -rf "$INSTALL_DIR"
    git clone https://github.com/vozlemetro/hash256-miner.git "$INSTALL_DIR"
    cd "$INSTALL_DIR"
fi

# Step 4: Build
log "Building miner..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
cmake --build . --clean-first 2>&1 | tail -3

# Verify
if [ ! -f hash256_cuda ]; then
    err "Build failed! Check errors above."
    exit 1
fi

log "Build successful!"
echo ""

# Done
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Installation complete!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "  To start mining, run:"
echo ""
echo -e "  ${YELLOW}cd ~/hash256-miner/build${NC}"
echo -e "  ${YELLOW}./hash256_cuda --privkey YOUR_KEY --rpc https://ethereum-rpc.publicnode.com --grid-size 8192${NC}"
echo ""
echo "  Replace YOUR_KEY with your wallet private key."
echo "  Make sure you have ~0.01 ETH on the wallet for gas."
echo ""
echo "  GPU grid-size recommendations:"
echo "    GTX 1060:    --grid-size 8192"
echo "    RTX 2070:    --grid-size 10240"
echo "    RTX 3070:    --grid-size 16384"
echo "    RTX 3080:    --grid-size 24576"
echo "    RTX 4080:    --grid-size 24576"
echo "    RTX 4090:    --grid-size 32768"
echo ""
