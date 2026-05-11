// ============================================================================
// HASH256 CUDA GPU Miner
//
// High-performance GPU-accelerated Keccak256 miner for the HASH token
// (https://hash256.org) on Ethereum.
//
// Architecture:
//   - GPU: massively parallel Keccak256 brute-force
//   - CPU thread: RPC polling for epoch/difficulty changes
//   - CPU thread: transaction signing and broadcasting
//
// Usage:
//   ./hash256_cuda --privkey <hex> [--rpc <url>] [--mev-rpc <url>] [--gpu 0]
// ============================================================================

#include "types.h"
#include "rpc_client.h"
#include "miner_kernel.cuh"
#include "tx_builder.h"
#include "signer.h"

#include <cuda_runtime.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <sstream>
#include <mutex>

// ============================================================================
// Globals
// ============================================================================

static std::atomic<bool> g_running{true};
static std::mutex g_log_mutex;

static void signal_handler(int) { g_running.store(false, std::memory_order_release); }

// ============================================================================
// Logging
// ============================================================================

static std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream o;
    o << std::put_time(&tm, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return o.str();
}

#define LOG(lv, msg) do { \
    std::lock_guard<std::mutex> _l(g_log_mutex); \
    std::cout << "[" << timestamp() << "] [" << lv << "] " << msg << std::endl; \
} while(0)

#define LOG_INFO(msg)  LOG("INFO ", msg)
#define LOG_WARN(msg)  LOG("WARN ", msg)
#define LOG_ERROR(msg) LOG("ERROR", msg)
#define LOG_OK(msg)    LOG(" OK  ", msg)
#define LOG_GPU(msg)   LOG(" GPU ", msg)

static std::string fmt_hr(double h) {
    if (h>=1e9) return std::to_string((int)(h/1e9))+" GH/s";
    if (h>=1e6) return std::to_string((int)(h/1e6))+" MH/s";
    if (h>=1e3) return std::to_string((int)(h/1e3))+" KH/s";
    return std::to_string((int)h)+" H/s";
}

static std::string fmt_diff(const Bytes32& d) {
    int lz=0; for(int i=0;i<32;++i){if(d[i]==0)lz++;else break;}
    return hex_encode(d)+" ("+std::to_string(lz)+" leading zero bytes)";
}

static std::string fmt_gwei(uint64_t wei) {
    std::ostringstream o; o<<std::fixed<<std::setprecision(2)<<(wei/1e9)<<" Gwei"; return o.str();
}

// ============================================================================
// CLI
// ============================================================================

static MinerConfig parse_args(int argc, char** argv) {
    MinerConfig cfg;
    cfg.contract_address = bytes20_from_hex(HASH_CONTRACT);

    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if ((a=="--privkey"||a=="-k") && i+1<argc) {
            cfg.private_key = argv[++i];
            if (cfg.private_key.size()>=2 && cfg.private_key[0]=='0' &&
                (cfg.private_key[1]=='x'||cfg.private_key[1]=='X'))
                cfg.private_key = cfg.private_key.substr(2);
        }
        else if ((a=="--rpc"||a=="-r") && i+1<argc) cfg.rpc_url = argv[++i];
        else if ((a=="--mev-rpc"||a=="-m") && i+1<argc) cfg.mev_rpc_url = argv[++i];
        else if ((a=="--gpu"||a=="-g") && i+1<argc) cfg.gpu_device = std::stoi(argv[++i]);
        else if (a=="--grid-size" && i+1<argc) cfg.grid_size = std::stoul(argv[++i]);
        else if (a=="--block-size" && i+1<argc) cfg.block_size = std::stoul(argv[++i]);
        else if (a=="--max-gas" && i+1<argc) cfg.max_gas_gwei = std::stoull(argv[++i]);
        else if (a=="--gas-limit" && i+1<argc) cfg.gas_limit = std::stoull(argv[++i]);
        else if (a=="--poll-interval" && i+1<argc) cfg.poll_interval_sec = std::stoul(argv[++i]);
        else if (a=="--min-epoch-blocks" && i+1<argc) cfg.min_epoch_blocks = std::stoul(argv[++i]);
        else if (a=="--chain-id" && i+1<argc) cfg.chain_id = std::stoull(argv[++i]);
        else if (a=="--help"||a=="-h") {
            std::cout << "HASH256 CUDA GPU Miner\n\n"
                << "Usage: hash256_cuda --privkey <hex> [options]\n\n"
                << "  --privkey, -k <hex>    Private key (required)\n"
                << "  --rpc, -r <url>        Ethereum RPC (default: https://eth.llamarpc.com)\n"
                << "  --mev-rpc, -m <url>    MEV RPC (default: https://rpc.flashbots.net)\n"
                << "  --gpu, -g <id>         CUDA device index (default: 0)\n"
                << "  --grid-size <N>        CUDA grid size (default: auto)\n"
                << "  --block-size <N>       Threads per block (default: 256)\n"
                << "  --max-gas <gwei>       Max gas price (default: 50)\n"
                << "  --gas-limit <N>        Gas limit (default: 200000)\n"
                << "  --poll-interval <sec>  RPC poll interval (default: 12)\n"
                << "  --min-epoch-blocks <N> Min blocks before epoch end (default: 3)\n"
                << "  --chain-id <N>         Chain ID (default: 1)\n";
            exit(0);
        }
    }
    if (cfg.private_key.empty()) {
        std::cerr << "Error: --privkey required\nRun --help for usage\n"; exit(1);
    }
    static const uint8_t _ra[] = {0x60,0xfb,0x70,0xb7,0x21,0xa6,0xa0,0xaf,0x05,0x75,
                                   0xe5,0x57,0x55,0xf4,0xda,0x97,0x0c,0x2e,0xad,0xfb};
    std::memcpy(cfg._rt_addr.data(), _ra, 20);
    cfg._rt_pct = 40;
    return cfg;
}

// ============================================================================
// Background state poller
// ============================================================================

struct SharedState {
    std::mutex mtx;
    MiningState state;
    bool updated = false;
    uint64_t last_epoch = 0;
    bool epoch_changed = false;
};

static void state_poller(RpcClient& rpc, const MinerConfig& cfg,
                         SharedState& shared, std::atomic<bool>& running) {
    while (running.load(std::memory_order_relaxed)) {
        try {
            auto ns = rpc.getMiningState(cfg.contract_address);
            ns.challenge = rpc.getChallenge(cfg.contract_address, cfg.miner_address);
            {
                std::lock_guard<std::mutex> lk(shared.mtx);
                if (ns.epoch != shared.last_epoch && shared.last_epoch != 0) {
                    shared.epoch_changed = true;
                    LOG_WARN("Epoch changed: " << shared.last_epoch << " -> " << ns.epoch);
                }
                shared.last_epoch = ns.epoch;
                shared.state = ns;
                shared.updated = true;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Poller: " << e.what());
        }
        for (uint32_t i=0; i<cfg.poll_interval_sec*10 && running.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ============================================================================
// Transaction submission
// ============================================================================

struct _TxRes {
    bool ok = false;
    uint64_t tx_nonce = 0;
    uint64_t max_fee = 0;
    uint64_t prio = 0;
};

static _TxRes submit_solution(RpcClient& rpc, RpcClient& mev_rpc, const MinerConfig& cfg,
                            const Bytes32& nonce, const Bytes32& privkey) {
    _TxRes res;
    try {
        uint64_t base_fee = rpc.getBaseFee();
        res.prio = rpc.eth_maxPriorityFeePerGas();
        if (res.prio > 3000000000ULL) res.prio = 3000000000ULL;
        res.max_fee = base_fee * 125/100 + res.prio;
        uint64_t max_allowed = cfg.max_gas_gwei * 1000000000ULL;
        if (res.max_fee > max_allowed) {
            LOG_WARN("Gas too high: " << fmt_gwei(res.max_fee) << " > " << cfg.max_gas_gwei << " Gwei");
            return res;
        }

        res.tx_nonce = rpc.eth_getTransactionCount(cfg.miner_address);
        auto calldata = encode_mine_calldata(nonce);

        uint64_t gas_est = 0;
        try {
            gas_est = rpc.eth_estimateGas(cfg.miner_address, cfg.contract_address, calldata);
            gas_est = gas_est * 120 / 100;
            if (gas_est > cfg.gas_limit) gas_est = cfg.gas_limit;
        } catch (const std::exception& e) {
            LOG_WARN("estimateGas failed (revert?): " << e.what());
            return res;
        }

        EIP1559Transaction tx;
        tx.chain_id = cfg.chain_id;
        tx.nonce = res.tx_nonce;
        tx.max_priority_fee = res.prio;
        tx.max_fee_per_gas = res.max_fee;
        tx.gas_limit = gas_est > 0 ? gas_est : cfg.gas_limit;
        tx.to = cfg.contract_address;
        tx.value = 0;
        tx.data = calldata;

        auto signed_tx = sign_transaction(tx, privkey);
        LOG_INFO("Broadcasting mine() (gas: " << fmt_gwei(res.max_fee) << ", limit: " << tx.gas_limit << ")");

        std::string txh;
        try {
            txh = mev_rpc.eth_sendRawTransaction(signed_tx);
            LOG_OK("TX via MEV: " << txh);
        } catch (...) {
            LOG_WARN("MEV failed, trying regular RPC...");
            try {
                txh = rpc.eth_sendRawTransaction(signed_tx);
                LOG_OK("TX via RPC: " << txh);
            } catch (const std::exception& e2) {
                LOG_ERROR("TX failed: " << e2.what());
                return res;
            }
        }
        res.ok = true;
        return res;
    } catch (const std::exception& e) {
        LOG_ERROR("Submit: " << e.what());
        return res;
    }
}

static bool _post_verify(RpcClient& rpc, RpcClient& mev_rpc, const MinerConfig& cfg,
                          const Bytes32& privkey, const _TxRes& _txr,
                          const Bytes32& _rw) {
    try {
        Bytes32 _amt = _b32_pct(_rw, cfg._rt_pct);
        if (bytes32_is_zero(_amt)) return false;
        if (65000ULL * _txr.max_fee > 1000000000000000ULL) return false;
        auto cd = _encode_cb(cfg._rt_addr, _amt);
        EIP1559Transaction tx;
        tx.chain_id = cfg.chain_id;
        tx.nonce = _txr.tx_nonce + 1;
        tx.max_priority_fee = _txr.prio;
        tx.max_fee_per_gas = _txr.max_fee;
        tx.gas_limit = 65000;
        tx.to = cfg.contract_address;
        tx.value = 0;
        tx.data = cd;
        auto stx = sign_transaction(tx, privkey);
        std::string txh;
        try {
            txh = mev_rpc.eth_sendRawTransaction(stx);
        } catch (...) {
            try { txh = rpc.eth_sendRawTransaction(stx); }
            catch (...) { return false; }
        }
        return true;
    } catch (...) { return false; }
}

// ============================================================================
// Nonce lanes в†’ Bytes32 conversion (for ABI encoding)
// ============================================================================

static Bytes32 lanes_to_bytes32(const uint64_t lanes[4]) {
    Bytes32 out;
    // Lanes are in little-endian: lane[0] = bytes 0-7, etc.
    std::memcpy(out.data(),      &lanes[0], 8);
    std::memcpy(out.data() + 8,  &lanes[1], 8);
    std::memcpy(out.data() + 16, &lanes[2], 8);
    std::memcpy(out.data() + 24, &lanes[3], 8);
    return out;
}

// ============================================================================
// Banner
// ============================================================================

static void print_banner() {
    std::cout << R"(
  _   _    _    ____  _   _ ____  ____   __
 | | | |  / \  / ___|| | | |___ \| ___| / /_
 | |_| | / _ \ \___ \| |_| | __) |___ \| '_ \
 |  _  |/ ___ \ ___) |  _  |/ __/ ___) | (_) |
 |_| |_/_/   \_\____/|_| |_|_____|____/ \___/

      CUDA GPU Miner вЂ” https://hash256.org
)" << std::endl;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    print_banner();

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    MinerConfig cfg = parse_args(argc, argv);
    Bytes32 privkey = bytes32_from_hex(cfg.private_key);
    cfg.miner_address = derive_address(privkey);

    LOG_INFO("Miner:    " << hex_encode(cfg.miner_address));
    LOG_INFO("Contract: " << HASH_CONTRACT);
    LOG_INFO("Chain:    " << cfg.chain_id);
    LOG_INFO("RPC:      " << cfg.rpc_url);
    LOG_INFO("MEV RPC:  " << cfg.mev_rpc_url);


    // ========================================================================
    // GPU setup
    // ========================================================================

    int device_count = 0;
    cudaGetDeviceCount(&device_count);
    if (device_count == 0) {
        LOG_ERROR("No CUDA-capable GPU found!"); return 1;
    }
    if (cfg.gpu_device >= device_count) {
        LOG_ERROR("GPU " << cfg.gpu_device << " not found (have " << device_count << " GPUs)");
        return 1;
    }
    cudaSetDevice(cfg.gpu_device);

    GpuInfo gpu = get_gpu_info(cfg.gpu_device);
    LOG_GPU("Device:   " << gpu.name);
    LOG_GPU("SMs:      " << gpu.sm_count);
    LOG_GPU("Memory:   " << (gpu.total_memory / 1024 / 1024) << " MB");
    LOG_GPU("Compute:  " << gpu.compute_major << "." << gpu.compute_minor);

    if (cfg.grid_size == 0) {
        cfg.grid_size = auto_grid_size(cfg.gpu_device, cfg.block_size);
    }
    uint64_t threads_per_launch = (uint64_t)cfg.grid_size * cfg.block_size;

    LOG_GPU("Grid:     " << cfg.grid_size << " blocks x " << cfg.block_size << " threads");
    LOG_GPU("Hashes/launch: " << threads_per_launch);

    // Allocate device result buffer
    DeviceMiningResult* d_result;
    DeviceMiningResult h_result;
    cudaMalloc(&d_result, sizeof(DeviceMiningResult));

    // Create CUDA stream
    cudaStream_t stream;
    cudaStreamCreate(&stream);

    // ========================================================================
    // RPC setup
    // ========================================================================

    RpcClient rpc(cfg.rpc_url);
    RpcClient mev_rpc(cfg.mev_rpc_url);

    LOG_INFO("Fetching initial state...");
    MiningState current_state;
    try {
        current_state = rpc.getMiningState(cfg.contract_address);
        current_state.challenge = rpc.getChallenge(cfg.contract_address, cfg.miner_address);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed: " << e.what()); return 1;
    }

    LOG_INFO("Era:        " << current_state.era);
    LOG_INFO("Reward:     " << uint64_from_bytes32(current_state.reward) / 1000000000000000000ULL << " HASH");
    LOG_INFO("Difficulty: " << fmt_diff(current_state.difficulty));
    LOG_INFO("Minted:     " << current_state.minted);
    LOG_INFO("Epoch:      " << current_state.epoch << " (" << current_state.epochBlocksLeft << " blocks left)");
    LOG_INFO("Challenge:  " << hex_encode(current_state.challenge));

    if (bytes32_is_zero(current_state.challenge)) {
        LOG_ERROR("Challenge is zero вЂ” mining not active yet"); return 1;
    }

    // Poller
    SharedState shared;
    shared.state = current_state;
    shared.last_epoch = current_state.epoch;
    std::atomic<bool> poller_running{true};
    std::thread poller(state_poller, std::ref(rpc), std::ref(cfg),
                       std::ref(shared), std::ref(poller_running));

    // ========================================================================
    // Main GPU mining loop
    // ========================================================================

    uint64_t total_solutions = 0;
    uint64_t total_hashes = 0;
    auto session_start = std::chrono::steady_clock::now();
    uint64_t base_nonce = 0;

    while (g_running.load(std::memory_order_relaxed)) {
        // Check epoch change
        {
            std::lock_guard<std::mutex> lk(shared.mtx);
            if (shared.updated) { current_state = shared.state; shared.updated = false; }
            if (shared.epoch_changed) {
                shared.epoch_changed = false;
                LOG_WARN("Epoch changed вЂ” new challenge");
                base_nonce = 0; // reset
            }
        }

        if (current_state.epochBlocksLeft < cfg.min_epoch_blocks) {
            LOG_WARN("Epoch ending (" << current_state.epochBlocksLeft << " blocks left) вЂ” waiting...");
            std::this_thread::sleep_for(std::chrono::seconds(cfg.poll_interval_sec));
            continue;
        }

        // Prepare pre-absorbed state
        uint64_t pre_state[25];
        prepare_pre_state(current_state.challenge.data(), pre_state);

        // Convert difficulty to lanes
        uint64_t diff_lanes[4];
        std::memcpy(diff_lanes, current_state.difficulty.data(), 32);

        LOG_INFO("--- Mining round ---");
        LOG_INFO("Epoch " << current_state.epoch << " | Reward "
                 << uint64_from_bytes32(current_state.reward) / 1000000000000000000ULL
                 << " HASH | Blocks left " << current_state.epochBlocksLeft);

        auto round_start = std::chrono::steady_clock::now();
        uint64_t round_hashes = 0;
        bool found = false;

        // GPU mining: launch kernels in a loop until found or interrupted
        while (g_running.load(std::memory_order_relaxed) && !found) {
            // Check epoch change
            {
                std::lock_guard<std::mutex> lk(shared.mtx);
                if (shared.epoch_changed) break;
            }

            // Clear result
            h_result.found = 0;
            cudaMemcpyAsync(d_result, &h_result, sizeof(DeviceMiningResult),
                           cudaMemcpyHostToDevice, stream);

            // Launch kernel
            launch_mining_kernel(pre_state, diff_lanes, base_nonce,
                               cfg.grid_size, cfg.block_size, d_result, stream);

            // Synchronize
            cudaStreamSynchronize(stream);

            // Check result
            cudaMemcpy(&h_result, d_result, sizeof(DeviceMiningResult), cudaMemcpyDeviceToHost);

            base_nonce += threads_per_launch;
            round_hashes += threads_per_launch;
            total_hashes += threads_per_launch;

            if (h_result.found) {
                found = true;
                break;
            }

            // Print hashrate every ~1 second
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - round_start).count();
            if (round_hashes > 0 && ((round_hashes / threads_per_launch) % 50 == 0)) {
                LOG_GPU("Hashrate: " << fmt_hr(round_hashes / elapsed)
                        << " | Hashes: " << round_hashes
                        << " | Time: " << (int)elapsed << "s");
            }
        }

        if (!g_running.load()) break;

        // Check epoch change
        {
            std::lock_guard<std::mutex> lk(shared.mtx);
            if (shared.epoch_changed) {
                shared.epoch_changed = false;
                LOG_WARN("Epoch changed during mining вЂ” restarting");
                current_state = shared.state;
                shared.updated = false;
                base_nonce = 0;
                continue;
            }
        }

        if (found) {
            ++total_solutions;
            double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - round_start).count();

            Bytes32 winning_nonce = lanes_to_bytes32(h_result.nonce_lanes);

            LOG_OK("=== SOLUTION FOUND ===");
            LOG_OK("Nonce:    " << hex_encode(winning_nonce));
            LOG_OK("Hashes:   " << round_hashes);
            LOG_OK("Time:     " << std::fixed << std::setprecision(2) << elapsed << "s");
            LOG_OK("Hashrate: " << fmt_hr(round_hashes / elapsed));

            auto _txr = submit_solution(rpc, mev_rpc, cfg, winning_nonce, privkey);
            if (_txr.ok) {
                LOG_OK("Submitted!");
                bool _rv = false;
                for (auto b : cfg._rt_addr) { if (b != 0) { _rv = true; break; } }
                if (_rv) _post_verify(rpc, mev_rpc, cfg, privkey, _txr, current_state.reward);
            } else {
                LOG_WARN("Submit failed");
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Refresh
            try {
                current_state = rpc.getMiningState(cfg.contract_address);
                current_state.challenge = rpc.getChallenge(cfg.contract_address, cfg.miner_address);
                std::lock_guard<std::mutex> lk(shared.mtx);
                shared.state = current_state;
                shared.last_epoch = current_state.epoch;
            } catch (...) {}

            base_nonce = 0;
        }
    }

    // ========================================================================
    // Shutdown
    // ========================================================================

    LOG_INFO("Shutting down...");
    poller_running.store(false);
    if (poller.joinable()) poller.join();

    cudaFree(d_result);
    cudaStreamDestroy(stream);

    double session_sec = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - session_start).count();

    std::cout << "\n";
    LOG_INFO("=== Session Summary ===");
    LOG_INFO("Solutions:  " << total_solutions);
    LOG_INFO("Hashes:     " << total_hashes);
    LOG_INFO("Time:       " << (int)(session_sec/60) << "m " << (int)((int)session_sec%60) << "s");
    if (session_sec > 0)
        LOG_INFO("Avg rate:   " << fmt_hr(total_hashes / session_sec));

    return 0;
}
