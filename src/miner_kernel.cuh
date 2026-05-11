#pragma once
#include <cstdint>

struct DeviceMiningResult {
    uint32_t found;
    uint64_t nonce_lanes[4];
};

void prepare_pre_state(const uint8_t challenge[32], uint64_t pre_state[25]);

void launch_mining_kernel(
    const uint64_t pre_state[25],
    const uint64_t difficulty_lanes[4],
    uint64_t base_nonce,
    uint32_t grid_size,
    uint32_t block_size,
    DeviceMiningResult* d_result,
    void* stream);

struct GpuInfo {
    char name[256];
    int sm_count;
    int max_threads_per_sm;
    int max_threads_per_block;
    size_t total_memory;
    int compute_major;
    int compute_minor;
};

GpuInfo get_gpu_info(int device);
uint32_t auto_grid_size(int device, uint32_t block_size);
