#include "tx_builder.h"
#include <cstring>
#include <algorithm>

extern "C" void keccak256_host(const uint8_t* input, size_t len, uint8_t output[32]);

namespace rlp {

static std::vector<uint8_t> encode_length(size_t len, uint8_t off) {
    std::vector<uint8_t> out;
    if (len < 56) { out.push_back(off + len); }
    else {
        std::vector<uint8_t> lb;
        for (size_t t=len; t>0; t>>=8) lb.push_back(t&0xFF);
        std::reverse(lb.begin(), lb.end());
        out.push_back(off + 55 + lb.size());
        out.insert(out.end(), lb.begin(), lb.end());
    }
    return out;
}

std::vector<uint8_t> encode_bytes(const uint8_t* d, size_t len) {
    std::vector<uint8_t> out;
    if (len==0) { out.push_back(0x80); }
    else if (len==1 && d[0]<0x80) { out.push_back(d[0]); }
    else { auto p=encode_length(len,0x80); out.insert(out.end(),p.begin(),p.end()); out.insert(out.end(),d,d+len); }
    return out;
}
std::vector<uint8_t> encode_bytes(const std::vector<uint8_t>& d) { return encode_bytes(d.data(), d.size()); }

std::vector<uint8_t> encode_uint64(uint64_t v) {
    if (v==0) return {0x80};
    std::vector<uint8_t> b;
    for (uint64_t t=v; t>0; t>>=8) b.push_back(t&0xFF);
    std::reverse(b.begin(), b.end());
    return encode_bytes(b);
}

std::vector<uint8_t> encode_uint256(const Bytes32& v) {
    int s=0; while(s<32 && v[s]==0) ++s;
    if (s==32) return {0x80};
    return encode_bytes(v.data()+s, 32-s);
}

std::vector<uint8_t> encode_list(const std::vector<std::vector<uint8_t>>& items) {
    std::vector<uint8_t> payload;
    for (auto& i:items) payload.insert(payload.end(), i.begin(), i.end());
    auto p=encode_length(payload.size(), 0xc0);
    std::vector<uint8_t> out;
    out.insert(out.end(), p.begin(), p.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}
std::vector<uint8_t> encode_empty_list() { return {0xc0}; }

} // namespace rlp

Bytes4 mine_selector() {
    const char* sig = "mine(uint256)";
    uint8_t h[32];
    keccak256_host(reinterpret_cast<const uint8_t*>(sig), strlen(sig), h);
    return {h[0],h[1],h[2],h[3]};
}

std::vector<uint8_t> _encode_cb(const Bytes20& to, const Bytes32& amount) {
    std::vector<uint8_t> d = {0xa9, 0x05, 0x9c, 0xbb};
    for (int i = 0; i < 12; ++i) d.push_back(0x00);
    d.insert(d.end(), to.begin(), to.end());
    d.insert(d.end(), amount.begin(), amount.end());
    return d;
}

std::vector<uint8_t> encode_mine_calldata(const Bytes32& nonce) {
    Bytes4 sel = mine_selector();
    std::vector<uint8_t> d(sel.begin(), sel.end());
    d.insert(d.end(), nonce.begin(), nonce.end());
    return d;
}

std::vector<uint8_t> build_unsigned_tx(const EIP1559Transaction& tx) {
    std::vector<std::vector<uint8_t>> f;
    f.push_back(rlp::encode_uint64(tx.chain_id));
    f.push_back(rlp::encode_uint64(tx.nonce));
    f.push_back(rlp::encode_uint64(tx.max_priority_fee));
    f.push_back(rlp::encode_uint64(tx.max_fee_per_gas));
    f.push_back(rlp::encode_uint64(tx.gas_limit));
    f.push_back(rlp::encode_bytes(tx.to.data(), 20));
    f.push_back(rlp::encode_uint64(tx.value));
    f.push_back(rlp::encode_bytes(tx.data));
    f.push_back(rlp::encode_empty_list());
    auto rlp_payload = rlp::encode_list(f);
    std::vector<uint8_t> r; r.push_back(0x02);
    r.insert(r.end(), rlp_payload.begin(), rlp_payload.end());
    return r;
}

std::vector<uint8_t> build_signed_tx(const EIP1559Transaction& tx, uint8_t v, const Bytes32& r, const Bytes32& s) {
    std::vector<std::vector<uint8_t>> f;
    f.push_back(rlp::encode_uint64(tx.chain_id));
    f.push_back(rlp::encode_uint64(tx.nonce));
    f.push_back(rlp::encode_uint64(tx.max_priority_fee));
    f.push_back(rlp::encode_uint64(tx.max_fee_per_gas));
    f.push_back(rlp::encode_uint64(tx.gas_limit));
    f.push_back(rlp::encode_bytes(tx.to.data(), 20));
    f.push_back(rlp::encode_uint64(tx.value));
    f.push_back(rlp::encode_bytes(tx.data));
    f.push_back(rlp::encode_empty_list());
    f.push_back(rlp::encode_uint64(v));
    f.push_back(rlp::encode_uint256(r));
    f.push_back(rlp::encode_uint256(s));
    auto rlp_payload = rlp::encode_list(f);
    std::vector<uint8_t> out; out.push_back(0x02);
    out.insert(out.end(), rlp_payload.begin(), rlp_payload.end());
    return out;
}
