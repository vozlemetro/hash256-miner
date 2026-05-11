#pragma once

#include <cstdint>

__device__ __constant__ static const uint64_t d_RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL,
    0x8000000080008000ULL, 0x000000000000808BULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008AULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800AULL, 0x800000008000000AULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

__device__ __forceinline__ uint64_t rotl64_dev(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

__device__ __forceinline__ void keccak_f1600_dev(uint64_t A[25]) {
    uint64_t C[5], D[5], t;

    #pragma unroll
    for (int round = 0; round < 24; ++round) {
        C[0] = A[0] ^ A[5] ^ A[10] ^ A[15] ^ A[20];
        C[1] = A[1] ^ A[6] ^ A[11] ^ A[16] ^ A[21];
        C[2] = A[2] ^ A[7] ^ A[12] ^ A[17] ^ A[22];
        C[3] = A[3] ^ A[8] ^ A[13] ^ A[18] ^ A[23];
        C[4] = A[4] ^ A[9] ^ A[14] ^ A[19] ^ A[24];

        D[0] = C[4] ^ rotl64_dev(C[1], 1);
        D[1] = C[0] ^ rotl64_dev(C[2], 1);
        D[2] = C[1] ^ rotl64_dev(C[3], 1);
        D[3] = C[2] ^ rotl64_dev(C[4], 1);
        D[4] = C[3] ^ rotl64_dev(C[0], 1);

        A[0]  ^= D[0]; A[5]  ^= D[0]; A[10] ^= D[0]; A[15] ^= D[0]; A[20] ^= D[0];
        A[1]  ^= D[1]; A[6]  ^= D[1]; A[11] ^= D[1]; A[16] ^= D[1]; A[21] ^= D[1];
        A[2]  ^= D[2]; A[7]  ^= D[2]; A[12] ^= D[2]; A[17] ^= D[2]; A[22] ^= D[2];
        A[3]  ^= D[3]; A[8]  ^= D[3]; A[13] ^= D[3]; A[18] ^= D[3]; A[23] ^= D[3];
        A[4]  ^= D[4]; A[9]  ^= D[4]; A[14] ^= D[4]; A[19] ^= D[4]; A[24] ^= D[4];

        t = A[1];
        A[ 1] = rotl64_dev(A[ 6], 44); A[ 6] = rotl64_dev(A[ 9], 20);
        A[ 9] = rotl64_dev(A[22], 61); A[22] = rotl64_dev(A[14], 39);
        A[14] = rotl64_dev(A[20], 18); A[20] = rotl64_dev(A[ 2], 62);
        A[ 2] = rotl64_dev(A[12], 43); A[12] = rotl64_dev(A[13], 25);
        A[13] = rotl64_dev(A[19],  8); A[19] = rotl64_dev(A[23], 56);
        A[23] = rotl64_dev(A[15], 41); A[15] = rotl64_dev(A[ 4], 27);
        A[ 4] = rotl64_dev(A[24], 14); A[24] = rotl64_dev(A[21],  2);
        A[21] = rotl64_dev(A[ 8], 55); A[ 8] = rotl64_dev(A[16], 45);
        A[16] = rotl64_dev(A[ 5], 36); A[ 5] = rotl64_dev(A[ 3], 28);
        A[ 3] = rotl64_dev(A[18], 21); A[18] = rotl64_dev(A[17], 15);
        A[17] = rotl64_dev(A[11], 10); A[11] = rotl64_dev(A[ 7],  6);
        A[ 7] = rotl64_dev(A[10],  3); A[10] = rotl64_dev(t, 1);

        #pragma unroll 5
        for (int i = 0; i < 25; i += 5) {
            uint64_t a0 = A[i], a1 = A[i+1], a2 = A[i+2], a3 = A[i+3], a4 = A[i+4];
            A[i]   = a0 ^ ((~a1) & a2);
            A[i+1] = a1 ^ ((~a2) & a3);
            A[i+2] = a2 ^ ((~a3) & a4);
            A[i+3] = a3 ^ ((~a4) & a0);
            A[i+4] = a4 ^ ((~a0) & a1);
        }

        A[0] ^= d_RC[round];
    }
}

__device__ __forceinline__ void keccak256_mine(
    const uint64_t pre_state[25],
    const uint64_t nonce_lanes[4],
    uint64_t out_hash[4])
{
    uint64_t A[25];
    #pragma unroll
    for (int i = 0; i < 25; ++i)
        A[i] = pre_state[i];

    A[4] ^= nonce_lanes[0];
    A[5] ^= nonce_lanes[1];
    A[6] ^= nonce_lanes[2];
    A[7] ^= nonce_lanes[3];

    keccak_f1600_dev(A);

    out_hash[0] = A[0];
    out_hash[1] = A[1];
    out_hash[2] = A[2];
    out_hash[3] = A[3];
}

#ifdef __cplusplus
extern "C" {
#endif
void keccak256_host(const uint8_t* input, size_t len, uint8_t output[32]);
#ifdef __cplusplus
}
#endif
