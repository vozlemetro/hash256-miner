#include "signer.h"
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <cstring>
#include <stdexcept>

extern "C" void keccak256_host(const uint8_t* input, size_t len, uint8_t output[32]);

static secp256k1_context* get_ctx() {
    static secp256k1_context* ctx = secp256k1_context_create(
        SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    return ctx;
}

Bytes20 derive_address(const Bytes32& pk) {
    auto* ctx = get_ctx();
    if (!secp256k1_ec_seckey_verify(ctx, pk.data()))
        throw std::runtime_error("Invalid private key");
    secp256k1_pubkey pub;
    if (!secp256k1_ec_pubkey_create(ctx, &pub, pk.data()))
        throw std::runtime_error("Failed to create pubkey");
    uint8_t ser[65]; size_t len=65;
    secp256k1_ec_pubkey_serialize(ctx, ser, &len, &pub, SECP256K1_EC_UNCOMPRESSED);
    uint8_t h[32];
    keccak256_host(ser + 1, 64, h);
    Bytes20 addr;
    std::memcpy(addr.data(), h + 12, 20);
    return addr;
}

ECDSASignature ecdsa_sign(const Bytes32& hash, const Bytes32& pk) {
    auto* ctx = get_ctx();
    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(ctx, &sig, hash.data(), pk.data(), nullptr, nullptr))
        throw std::runtime_error("Sign failed");
    ECDSASignature r;
    uint8_t out[64]; int recid;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, out, &recid, &sig);
    r.v = (uint8_t)recid;
    std::memcpy(r.r.data(), out, 32);
    std::memcpy(r.s.data(), out + 32, 32);
    return r;
}

std::vector<uint8_t> sign_transaction(const EIP1559Transaction& tx, const Bytes32& pk) {
    auto payload = build_unsigned_tx(tx);
    Bytes32 hash;
    keccak256_host(payload.data(), payload.size(), hash.data());
    auto sig = ecdsa_sign(hash, pk);
    return build_signed_tx(tx, sig.v, sig.r, sig.s);
}
