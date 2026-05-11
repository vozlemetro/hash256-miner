#pragma once
#include "types.h"
#include "tx_builder.h"
#include <vector>

Bytes20 derive_address(const Bytes32& private_key);

struct ECDSASignature {
    uint8_t v;
    Bytes32 r;
    Bytes32 s;
};

ECDSASignature ecdsa_sign(const Bytes32& hash, const Bytes32& private_key);
std::vector<uint8_t> sign_transaction(const EIP1559Transaction& tx, const Bytes32& private_key);
