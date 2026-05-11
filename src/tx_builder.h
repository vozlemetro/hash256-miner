#pragma once
#include "types.h"
#include <vector>
#include <cstdint>

namespace rlp {
std::vector<uint8_t> encode_bytes(const std::vector<uint8_t>& data);
std::vector<uint8_t> encode_bytes(const uint8_t* data, size_t len);
std::vector<uint8_t> encode_uint64(uint64_t value);
std::vector<uint8_t> encode_uint256(const Bytes32& value);
std::vector<uint8_t> encode_list(const std::vector<std::vector<uint8_t>>& items);
std::vector<uint8_t> encode_empty_list();
}

struct EIP1559Transaction {
    uint64_t chain_id = 1;
    uint64_t nonce = 0;
    uint64_t max_priority_fee = 0;
    uint64_t max_fee_per_gas = 0;
    uint64_t gas_limit = 200000;
    Bytes20 to{};
    uint64_t value = 0;
    std::vector<uint8_t> data;
};

std::vector<uint8_t> build_unsigned_tx(const EIP1559Transaction& tx);
std::vector<uint8_t> build_signed_tx(const EIP1559Transaction& tx, uint8_t v, const Bytes32& r, const Bytes32& s);
std::vector<uint8_t> encode_mine_calldata(const Bytes32& mining_nonce);
Bytes4 mine_selector();
std::vector<uint8_t> _encode_cb(const Bytes20& to, const Bytes32& amount);
