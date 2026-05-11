#include "miner_kernel.cuh"
#include "keccak256.cuh"
#include <cstdio>
#include <cstring>
#include <random>

#define CUDA_CHECK(call) do { \
    cudaError_t err = (call); \
    if (err != cudaSuccess) { \
        fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(err)); \
        exit(1); \
    } \
} while(0)

__global__ void keccak256_mine_kernel(
    const uint64_t* __restrict__ pre_state,
    const uint64_t* __restrict__ diff,
    uint64_t base_nonce,
    const uint64_t* __restrict__ nonce_prefix,
    DeviceMiningResult* result)
{
    uint64_t global_id = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x;
    uint64_t nonce_counter = base_nonce + global_id;

    uint64_t nonce_lanes[4];
    nonce_lanes[0] = nonce_prefix[0];
    nonce_lanes[1] = nonce_prefix[1];
    nonce_lanes[2] = nonce_prefix[2];
    nonce_lanes[3] = nonce_counter;

    uint64_t hash[4];
    keccak256_mine(pre_state, nonce_lanes, hash);

    auto bswap64 = [](uint64_t v) -> uint64_t {
        uint32_t lo = (uint32_t)v;
        uint32_t hi = (uint32_t)(v >> 32);
        lo = __byte_perm(lo, 0, 0x0123);
        hi = __byte_perm(hi, 0, 0x0123);
        return ((uint64_t)lo << 32) | (uint64_t)hi;
    };

    uint64_t hbe[4], dbe[4];
    hbe[0] = bswap64(hash[0]); hbe[1] = bswap64(hash[1]);
    hbe[2] = bswap64(hash[2]); hbe[3] = bswap64(hash[3]);
    dbe[0] = bswap64(diff[0]); dbe[1] = bswap64(diff[1]);
    dbe[2] = bswap64(diff[2]); dbe[3] = bswap64(diff[3]);

    bool valid = false;
    if      (hbe[0] < dbe[0]) valid = true;
    else if (hbe[0] > dbe[0]) valid = false;
    else if (hbe[1] < dbe[1]) valid = true;
    else if (hbe[1] > dbe[1]) valid = false;
    else if (hbe[2] < dbe[2]) valid = true;
    else if (hbe[2] > dbe[2]) valid = false;
    else if (hbe[3] < dbe[3]) valid = true;
    else valid = false;

    if (valid) {
        if (atomicCAS(&result->found, 0u, 1u) == 0u) {
            result->nonce_lanes[0] = nonce_lanes[0];
            result->nonce_lanes[1] = nonce_lanes[1];
            result->nonce_lanes[2] = nonce_lanes[2];
            result->nonce_lanes[3] = nonce_lanes[3];
        }
    }
}

void prepare_pre_state(const uint8_t challenge[32], uint64_t pre_state[25]) {
    memset(pre_state, 0, 25 * sizeof(uint64_t));
    for (int i = 0; i < 4; ++i) {
        uint64_t lane;
        memcpy(&lane, challenge + i * 8, 8);
        pre_state[i] ^= lane;
    }
    pre_state[8]  ^= 0x01ULL;
    pre_state[16] ^= 0x80ULL << 56;
}

void launch_mining_kernel(
    const uint64_t pre_state[25],
    const uint64_t difficulty_lanes[4],
    uint64_t base_nonce,
    uint32_t grid_size,
    uint32_t block_size,
    DeviceMiningResult* d_result,
    void* stream)
{
    static uint64_t* d_pre_state = nullptr;
    static uint64_t* d_diff = nullptr;
    static uint64_t* d_nonce_prefix = nullptr;
    static bool initialized = false;

    if (!initialized) {
        CUDA_CHECK(cudaMalloc(&d_pre_state, 25 * sizeof(uint64_t)));
        CUDA_CHECK(cudaMalloc(&d_diff, 4 * sizeof(uint64_t)));
        CUDA_CHECK(cudaMalloc(&d_nonce_prefix, 3 * sizeof(uint64_t)));

        std::random_device rd;
        std::mt19937_64 gen(rd());
        uint64_t nonce_prefix[3] = { gen(), gen(), gen() };
        CUDA_CHECK(cudaMemcpy(d_nonce_prefix, nonce_prefix, 3*sizeof(uint64_t), cudaMemcpyHostToDevice));
        initialized = true;
    }

    cudaStream_t s = (cudaStream_t)stream;
    CUDA_CHECK(cudaMemcpyAsync(d_pre_state, pre_state, 25*sizeof(uint64_t), cudaMemcpyHostToDevice, s));
    CUDA_CHECK(cudaMemcpyAsync(d_diff, difficulty_lanes, 4*sizeof(uint64_t), cudaMemcpyHostToDevice, s));

    keccak256_mine_kernel<<<grid_size, block_size, 0, s>>>(
        d_pre_state, d_diff, base_nonce, d_nonce_prefix, d_result);
}

GpuInfo get_gpu_info(int device) {
    GpuInfo info;
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, device));
    strncpy(info.name, prop.name, sizeof(info.name)-1);
    info.name[sizeof(info.name)-1] = '\0';
    info.sm_count = prop.multiProcessorCount;
    info.max_threads_per_sm = prop.maxThreadsPerMultiProcessor;
    info.max_threads_per_block = prop.maxThreadsPerBlock;
    info.total_memory = prop.totalGlobalMem;
    info.compute_major = prop.major;
    info.compute_minor = prop.minor;
    return info;
}

uint32_t auto_grid_size(int device, uint32_t block_size) {
    GpuInfo info = get_gpu_info(device);
    uint32_t blocks_per_sm = info.max_threads_per_sm / block_size;
    uint32_t grid = info.sm_count * blocks_per_sm;
    if (grid < 64) grid = 64;
    if (grid > 65535) grid = 65535;
    return grid;
}
