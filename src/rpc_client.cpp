#include "rpc_client.h"
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <iostream>

extern "C" void keccak256_host(const uint8_t* input, size_t len, uint8_t output[32]);

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* r = static_cast<std::string*>(ud);
    r->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string json_extract(const std::string& json, const std::string& key) {
    std::string s = "\"" + key + "\"";
    size_t p = json.find(s);
    if (p == std::string::npos) return "";
    p = json.find(':', p + s.size());
    if (p == std::string::npos) return "";
    p++;
    while (p < json.size() && (json[p]==' '||json[p]=='\t'||json[p]=='\n')) p++;
    if (p >= json.size()) return "";
    if (json[p] == '"') {
        size_t start = p+1, end = json.find('"', start);
        return end != std::string::npos ? json.substr(start, end-start) : "";
    }
    if (json[p] == 'n') return "null";
    size_t start = p;
    while (p < json.size() && json[p]!=',' && json[p]!='}' && json[p]!=']') p++;
    return json.substr(start, p-start);
}

static bool json_has_error(const std::string& j) {
    return j.find("\"error\"") != std::string::npos && j.find("\"error\":null") == std::string::npos;
}

RpcClient::RpcClient(const std::string& url) : url_(url) {
    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("Failed to init curl");
}
RpcClient::~RpcClient() { if (curl_) curl_easy_cleanup(static_cast<CURL*>(curl_)); }

std::string RpcClient::call(const std::string& method, const std::string& params) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto* c = static_cast<CURL*>(curl_);
    std::string resp;
    std::string body = "{\"jsonrpc\":\"2.0\",\"method\":\""+method+"\",\"params\":"+params+",\"id\":1}";
    struct curl_slist* h = nullptr;
    h = curl_slist_append(h, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    CURLcode res = curl_easy_perform(c);
    curl_slist_free_all(h);
    if (res != CURLE_OK) throw std::runtime_error(std::string("RPC: ") + curl_easy_strerror(res));
    if (json_has_error(resp)) throw std::runtime_error("RPC error: " + resp);
    return resp;
}

std::string RpcClient::extractResult(const std::string& r) { return json_extract(r, "result"); }

uint64_t RpcClient::parseHexU64(const std::string& hex) {
    std::string h = hex;
    if (h.size()>=2 && h[0]=='0' && (h[1]=='x'||h[1]=='X')) h=h.substr(2);
    if (h.empty()) return 0;
    return std::stoull(h, nullptr, 16);
}

std::string RpcClient::eth_call(const Bytes20& to, const std::vector<uint8_t>& data) {
    std::string p = "[{\"to\":\""+hex_encode(to)+"\",\"data\":\""+hex_encode(data)+"\"},\"latest\"]";
    return extractResult(call("eth_call", p));
}

uint64_t RpcClient::eth_getTransactionCount(const Bytes20& addr) {
    return parseHexU64(extractResult(call("eth_getTransactionCount","[\""+hex_encode(addr)+"\",\"pending\"]")));
}
uint64_t RpcClient::eth_blockNumber() { return parseHexU64(extractResult(call("eth_blockNumber","[]"))); }
uint64_t RpcClient::eth_gasPrice() { return parseHexU64(extractResult(call("eth_gasPrice","[]"))); }
uint64_t RpcClient::eth_maxPriorityFeePerGas() { return parseHexU64(extractResult(call("eth_maxPriorityFeePerGas","[]"))); }

uint64_t RpcClient::getBaseFee() {
    auto r = call("eth_getBlockByNumber","[\"latest\",false]");
    return parseHexU64(json_extract(r, "baseFeePerGas"));
}

std::string RpcClient::eth_sendRawTransaction(const std::vector<uint8_t>& tx) {
    return extractResult(call("eth_sendRawTransaction","[\""+hex_encode(tx)+"\"]"));
}

uint64_t RpcClient::eth_estimateGas(const Bytes20& from, const Bytes20& to,
                                     const std::vector<uint8_t>& data, uint64_t value) {
    std::string p = "[{\"from\":\""+hex_encode(from)+"\",\"to\":\""+hex_encode(to)+
                    "\",\"data\":\""+hex_encode(data)+"\"";
    if (value > 0) { std::ostringstream o; o<<"0x"<<std::hex<<value; p+=",\"value\":\""+o.str()+"\""; }
    p += "},\"latest\"]";
    return parseHexU64(extractResult(call("eth_estimateGas", p)));
}

static Bytes4 make_selector(const char* sig) {
    uint8_t h[32];
    keccak256_host(reinterpret_cast<const uint8_t*>(sig), strlen(sig), h);
    return {h[0],h[1],h[2],h[3]};
}

MiningState RpcClient::getMiningState(const Bytes20& contract) {
    static Bytes4 sel = make_selector("miningState()");
    std::vector<uint8_t> d(sel.begin(), sel.end());
    auto dec = hex_decode(eth_call(contract, d));
    if (dec.size() < 7*32) throw std::runtime_error("miningState(): bad response");
    MiningState s;
    auto ru64 = [&](int slot) -> uint64_t {
        uint64_t v=0; for(int i=24;i<32;++i) v=(v<<8)|dec[slot*32+i]; return v;
    };
    s.era=ru64(0);
    std::memcpy(s.reward.data(), dec.data()+1*32, 32);
    std::memcpy(s.difficulty.data(), dec.data()+2*32, 32);
    s.minted=ru64(3); s.remaining=ru64(4); s.epoch=ru64(5); s.epochBlocksLeft=ru64(6);
    return s;
}

Bytes32 RpcClient::getChallenge(const Bytes20& contract, const Bytes20& miner) {
    static Bytes4 sel = make_selector("getChallenge(address)");
    std::vector<uint8_t> d(sel.begin(), sel.end());
    for(int i=0;i<12;++i) d.push_back(0);
    d.insert(d.end(), miner.begin(), miner.end());
    return bytes32_from_hex(eth_call(contract, d));
}

Bytes32 RpcClient::getCurrentDifficulty(const Bytes20& contract) {
    static Bytes4 sel = make_selector("currentDifficulty()");
    std::vector<uint8_t> d(sel.begin(), sel.end());
    return bytes32_from_hex(eth_call(contract, d));
}
