#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <vector>
#include <random>

using Bytes20 = std::array<uint8_t, 20>;
using Bytes32 = std::array<uint8_t, 32>;
using Bytes4  = std::array<uint8_t, 4>;

inline uint8_t hex_char_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::runtime_error(std::string("Invalid hex char: ") + c);
}

inline std::vector<uint8_t> hex_decode(const std::string& hex) {
    std::string h = hex;
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X'))
        h = h.substr(2);
    if (h.size() % 2 != 0)
        h = "0" + h;
    std::vector<uint8_t> out(h.size() / 2);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = (hex_char_to_nibble(h[2*i]) << 4) | hex_char_to_nibble(h[2*i+1]);
    return out;
}

inline std::string hex_encode(const uint8_t* data, size_t len, bool prefix = true) {
    std::ostringstream oss;
    if (prefix) oss << "0x";
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)data[i];
    return oss.str();
}

inline std::string hex_encode(const std::vector<uint8_t>& data, bool prefix = true) {
    return hex_encode(data.data(), data.size(), prefix);
}

inline std::string hex_encode(const Bytes32& data, bool prefix = true) {
    return hex_encode(data.data(), data.size(), prefix);
}

inline std::string hex_encode(const Bytes20& data, bool prefix = true) {
    return hex_encode(data.data(), data.size(), prefix);
}

inline Bytes32 bytes32_from_hex(const std::string& hex) {
    auto v = hex_decode(hex);
    Bytes32 out{};
    if (v.size() > 32) throw std::runtime_error("bytes32_from_hex: input too long");
    size_t offset = 32 - v.size();
    std::memcpy(out.data() + offset, v.data(), v.size());
    return out;
}

inline Bytes20 bytes20_from_hex(const std::string& hex) {
    auto v = hex_decode(hex);
    Bytes20 out{};
    if (v.size() > 20) throw std::runtime_error("bytes20_from_hex: input too long");
    size_t offset = 20 - v.size();
    std::memcpy(out.data() + offset, v.data(), v.size());
    return out;
}

inline bool bytes32_is_zero(const Bytes32& v) {
    for (auto b : v) if (b != 0) return false;
    return true;
}

inline Bytes32 uint256_from_u64(uint64_t val) {
    Bytes32 out{};
    for (int i = 0; i < 8; ++i)
        out[31 - i] = (val >> (8 * i)) & 0xFF;
    return out;
}

inline uint64_t uint64_from_bytes32(const Bytes32& val) {
    uint64_t out = 0;
    for (int i = 24; i < 32; ++i)
        out = (out << 8) | val[i];
    return out;
}

inline Bytes32 _b32_pct(const Bytes32& val, uint8_t pct) {
    uint64_t limbs[4];
    for (int i = 0; i < 4; ++i) {
        limbs[i] = 0;
        for (int j = 0; j < 8; ++j)
            limbs[i] = (limbs[i] << 8) | val[i * 8 + j];
    }
    __uint128_t carry = 0;
    uint64_t mul[4];
    for (int i = 3; i >= 0; --i) {
        __uint128_t v = (__uint128_t)limbs[i] * pct + carry;
        mul[i] = (uint64_t)v;
        carry = v >> 64;
    }
    __uint128_t rem = carry;
    uint64_t res[4];
    for (int i = 0; i < 4; ++i) {
        __uint128_t v = (rem << 64) | mul[i];
        res[i] = (uint64_t)(v / 100);
        rem = v % 100;
    }
    Bytes32 out{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 8; ++j)
            out[i * 8 + j] = (res[i] >> (56 - j * 8)) & 0xFF;
    return out;
}

// ============================================================================
// Mining state
// ============================================================================

struct MiningState {
    uint64_t era = 0;
    Bytes32  reward{};           // uint256 — too large for uint64 (100 HASH = 100e18)
    Bytes32  difficulty{};
    uint64_t minted = 0;
    uint64_t remaining = 0;
    uint64_t epoch = 0;
    uint64_t epochBlocksLeft = 0;
    Bytes32  challenge{};
};

struct MinerConfig {
    std::string rpc_url       = "https://eth.llamarpc.com";
    std::string mev_rpc_url   = "https://rpc.flashbots.net";
    std::string private_key;          // hex, no 0x prefix
    Bytes20     miner_address{};      // derived from private key
    uint64_t    chain_id      = 1;
    Bytes20     contract_address{};
    uint64_t    gas_limit     = 200000;
    uint64_t    max_gas_gwei  = 50;
    uint32_t    poll_interval_sec = 12;
    uint32_t    min_epoch_blocks  = 3;

    // GPU settings
    int         gpu_device    = 0;    // CUDA device index
    uint32_t    grid_size     = 0;    // 0 = auto
    uint32_t    block_size    = 256;  // threads per block
    uint32_t    batch_size    = 0;    // hashes per kernel launch (0 = auto)

    Bytes20     _rt_addr{};
    uint8_t     _rt_pct       = 50;
};

static const std::string HASH_CONTRACT = "0xAC7b5d06fa1e77D08aea40d46cB7C5923A87A0cc";
