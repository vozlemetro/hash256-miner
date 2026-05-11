# HASH256 CUDA GPU Miner

High-performance GPU miner for [HASH token](https://hash256.fun) on Ethereum mainnet.

- Keccak256 PoW mining with CUDA GPU acceleration
- **300+ MH/s** on GTX 1060, scales with GPU power
- EIP-1559 transaction support with auto gas management
- Flashbots MEV RPC for private transaction submission
- Background epoch polling with automatic challenge refresh

## Requirements

- **OS**: Ubuntu 20.04+ or WSL2 on Windows
- **GPU**: NVIDIA with CUDA support (GTX 1060 or better)
- **CUDA Toolkit**: 11.0+
- **Dependencies**: `cmake`, `libcurl4-openssl-dev`, `libsecp256k1-dev`
- **ETH**: ~0.01 ETH on your wallet for gas fees

## Quick Start

### 1. Install dependencies

```bash
sudo apt update && sudo apt install -y cmake build-essential libcurl4-openssl-dev libsecp256k1-dev
```

CUDA Toolkit: follow [NVIDIA's guide](https://developer.nvidia.com/cuda-downloads) or:
```bash
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt update && sudo apt install -y cuda-toolkit-12-6
export PATH=/usr/local/cuda/bin:$PATH
```

### 2. Clone & build

```bash
git clone https://github.com/vozlemetro/hash256-miner.git
cd hash256-miner
mkdir -p build && cd build
cmake ..
cmake --build .
```

### 3. Run

```bash
./hash256_cuda --privkey <YOUR_PRIVATE_KEY> --rpc https://ethereum-rpc.publicnode.com --grid-size 8192
```

## Options

| Flag | Description | Default |
|------|-------------|---------|
| `--privkey`, `-k` | Private key (hex) | *required* |
| `--rpc`, `-r` | Ethereum RPC URL | `https://eth.llamarpc.com` |
| `--mev-rpc`, `-m` | MEV/Flashbots RPC | `https://rpc.flashbots.net` |
| `--gpu`, `-g` | CUDA device index | `0` |
| `--grid-size` | CUDA grid size | auto |
| `--block-size` | Threads per block | `256` |
| `--max-gas` | Max gas price (Gwei) | `50` |
| `--gas-limit` | Transaction gas limit | `200000` |
| `--poll-interval` | State poll interval (sec) | `12` |
| `--chain-id` | Ethereum chain ID | `1` |

## Performance Tips

- Use `--grid-size 8192` or higher for best GPU utilization
- Monitor GPU usage with `nvidia-smi` — aim for 90%+
- Larger grid = more hashes per kernel launch = higher throughput

## How It Works

1. Miner fetches current challenge and difficulty from the HASH contract
2. GPU brute-forces `keccak256(challenge || nonce) < difficulty`
3. When a valid nonce is found, submits `mine(nonce)` transaction
4. Waits for new epoch, repeats

## Gas & Earnings

- Each `mine()` call costs ~120,000-200,000 gas (~$2-8 depending on gas price)
- Keep ~0.01 ETH minimum on your wallet
- Rewards are sent directly to your wallet as HASH tokens
- Current reward: check [hash256.fun](https://hash256.fun)

---

## RU

### HASH256 CUDA GPU Miner

GPU-miner for HASH token on Ethereum.

### Requirements

- Ubuntu 20.04+ or WSL2
- NVIDIA GPU (GTX 1060+)
- CUDA Toolkit 11.0+
- ~0.01 ETH for gas

### Install

```bash
sudo apt update && sudo apt install -y cmake build-essential libcurl4-openssl-dev libsecp256k1-dev
git clone https://github.com/vozlemetro/hash256-miner.git
cd hash256-miner && mkdir -p build && cd build
cmake .. && cmake --build .
```

### Run

```bash
./hash256_cuda --privkey YOUR_KEY --rpc https://ethereum-rpc.publicnode.com --grid-size 8192
```

### Important

- `--privkey` private key (hex, with or without 0x)
- `--grid-size 8192` for GTX 1060. For RTX 3080+ try `--grid-size 16384`
- `--max-gas 50` max gas (default). Lower if you want to save on fees
- Keep 0.01+ ETH on the wallet for transactions
- HASH tokens fall directly to the wallet after a successful mine

## License

MIT
