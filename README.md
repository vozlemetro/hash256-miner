# HASH256 CUDA GPU Miner

High-performance GPU miner for [**HASH**](https://hash256.fun) — the first true Proof-of-Work token on Ethereum.

**HASH** is a fair-launch PoW token with Bitcoin-like mechanics: keccak256 mining, difficulty retargeting, halvings every 100k mints. No team allocation, no VC, no airdrop — 90% of supply is mined. The project launched recently and only ~8% has been mined so far. Tokens are already tradeable on Uniswap.

## Why GPU miner?

The official [hash256.fun/mine](https://hash256.fun/mine) page mines in the browser using WebAssembly — it works, but it's slow. This miner uses **CUDA GPU acceleration** and is **50-100x faster** than browser mining:

| Method | Hashrate | Notes |
|--------|----------|-------|
| Browser (WASM) | 2-5 MH/s | CPU-bound, single tab |
| **This miner (GPU)** | **300+ MH/s** | GTX 1060. RTX 3080 = 1+ GH/s |

I wrote this for myself and decided to share it — I want to grow the total network hashrate and build an active mining community around HASH. The more miners join, the stronger and more decentralized the network becomes. Feel free to use it, fork it, improve it.

## Token Info

| | |
|---|---|
| **Contract** | [0xAC7b5d06fa1e77D08aea40d46cB7C5923A87A0cc](https://etherscan.io/address/0xAC7b5d06fa1e77D08aea40d46cB7C5923A87A0cc) |
| **Total supply** | 21,000,000 HASH |
| **Mineable** | 18,900,000 (90%) |
| **Current reward** | 100 HASH per solution (Era 1) |
| **Halvings** | Every 100,000 mints |
| **Trade** | [Uniswap](https://app.uniswap.org/swap?outputCurrency=0xAC7b5d06fa1e77D08aea40d46cB7C5923A87A0cc) |
| **Website** | [hash256.fun](https://hash256.fun) |

## Requirements

- **OS**: Ubuntu 20.04+ or WSL2 on Windows 10/11
- **GPU**: NVIDIA with CUDA (GTX 1060 6GB or better)
- **CUDA Toolkit**: 11.0+
- **ETH**: ~0.01 ETH on your wallet for gas fees

## Quick Start (Windows + WSL2)

Most people use Windows, so here's the full setup:

### Step 1: Install WSL2 (if you don't have it)

Open **PowerShell as Administrator** and run:
```powershell
wsl --install -d Ubuntu-22.04
```
Restart your PC, then open **Ubuntu-22.04** from the Start menu. Set up a username and password.

### Step 2: Install CUDA Toolkit inside WSL

```bash
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt update
sudo apt install -y cuda-toolkit-12-6 cmake build-essential libcurl4-openssl-dev libsecp256k1-dev
```

Add CUDA to PATH (add this to your `~/.bashrc`):
```bash
export PATH=/usr/local/cuda/bin:$PATH
```
Then run `source ~/.bashrc`.

### Step 3: Clone and build

```bash
git clone https://github.com/vozlemetro/hash256-miner.git
cd hash256-miner
mkdir -p build && cd build
cmake ..
cmake --build .
```

### Step 4: Run

```bash
./hash256_cuda --privkey <YOUR_PRIVATE_KEY> --rpc https://ethereum-rpc.publicnode.com --grid-size 8192
```

You should see the banner, your address, GPU info, and "Mining round" logs with hashrate.

### Where to get a private key?

1. Install [Rabby Wallet](https://rabby.io/) or MetaMask
2. Create a new wallet
3. Export your private key (Settings > Export Private Key)
4. Send ~0.01 ETH to that wallet for gas

> **Warning**: Never share your private key. This miner runs locally on your machine — your key never leaves your computer.

## Options

| Flag | Description | Default |
|------|-------------|---------|
| `--privkey`, `-k` | Private key (hex) | *required* |
| `--rpc`, `-r` | Ethereum RPC URL | `https://eth.llamarpc.com` |
| `--mev-rpc`, `-m` | MEV/Flashbots RPC | `https://rpc.flashbots.net` |
| `--gpu`, `-g` | CUDA device index | `0` |
| `--grid-size` | CUDA grid blocks | auto |
| `--block-size` | Threads per block | `256` |
| `--max-gas` | Max gas (Gwei) | `50` |
| `--poll-interval` | State poll (sec) | `12` |

## Performance Tips

- **GTX 1060**: `--grid-size 8192` (~340 MH/s)
- **RTX 3070**: `--grid-size 16384` (~800 MH/s)
- **RTX 3080/4080**: `--grid-size 32768` (~1.2 GH/s)
- Monitor GPU with `nvidia-smi` — aim for 90%+ utilization

## How It Works

1. Fetches your wallet-specific challenge from the HASH contract
2. GPU brute-forces billions of nonces: `keccak256(challenge || nonce) < difficulty`
3. When a valid nonce is found, submits `mine(nonce)` on-chain
4. HASH tokens land directly in your wallet
5. Challenge refreshes every epoch (~20 min), repeat

## Gas Costs

- Each `mine()` tx costs ~120,000-200,000 gas
- At 5 Gwei = ~$1-3 per solution
- Keep 0.01+ ETH on your wallet
- Use `--max-gas 30` to skip expensive blocks

---

# RU

## HASH256 CUDA GPU Miner

GPU-miner for [**HASH**](https://hash256.fun) — a Proof-of-Work token on Ethereum.

**HASH** — fair-launch PoW token, like Bitcoin: keccak256 mining, difficulty retargeting, halvings. 0% team allocation — 90% supply mined. Project launched recently, only ~8% mined. Tokens tradeable on [Uniswap](https://app.uniswap.org/swap?outputCurrency=0xAC7b5d06fa1e77D08aea40d46cB7C5923A87A0cc).

## Why GPU miner?

Browser mining on [hash256.fun/mine](https://hash256.fun/mine) is 2-5 MH/s (WASM). This CUDA miner is **50-100x faster**:
- GTX 1060: **340 MH/s**
- RTX 3080+: **1+ GH/s**

Written for myself, sharing to grow the total hashrate and the HASH mining community. Open source, full code.

## Installation (Windows)

### 1. WSL2

PowerShell (admin):
```powershell
wsl --install -d Ubuntu-22.04
```
Reboot. Open Ubuntu-22.04.

### 2. CUDA + deps

```bash
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt update
sudo apt install -y cuda-toolkit-12-6 cmake build-essential libcurl4-openssl-dev libsecp256k1-dev
echo 'export PATH=/usr/local/cuda/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

### 3. Build

```bash
git clone https://github.com/vozlemetro/hash256-miner.git
cd hash256-miner && mkdir -p build && cd build
cmake .. && cmake --build .
```

### 4. Run

```bash
./hash256_cuda --privkey YOUR_KEY --rpc https://ethereum-rpc.publicnode.com --grid-size 8192
```

### Wallet

1. [Rabby Wallet](https://rabby.io/) or MetaMask
2. Create wallet, export private key
3. Send 0.01+ ETH for gas
4. Profit: HASH tokens go straight to your wallet

## Tips

- `--grid-size 8192` for GTX 1060, `16384` for RTX 3070+
- `--max-gas 30` to save on gas
- `nvidia-smi` to check GPU load (90%+ is ideal)
- HASH tokens can be sold on [Uniswap](https://app.uniswap.org/swap?outputCurrency=0xAC7b5d06fa1e77D08aea40d46cB7C5923A87A0cc)

## License

MIT
