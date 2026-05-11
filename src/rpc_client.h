#pragma once
#include "types.h"
#include <string>
#include <mutex>

class RpcClient {
public:
    explicit RpcClient(const std::string& url);
    ~RpcClient();
    std::string call(const std::string& method, const std::string& params);
    std::string eth_call(const Bytes20& to, const std::vector<uint8_t>& data);
    uint64_t eth_getTransactionCount(const Bytes20& address);
    uint64_t eth_blockNumber();
    uint64_t eth_gasPrice();
    uint64_t eth_maxPriorityFeePerGas();
    uint64_t getBaseFee();
    std::string eth_sendRawTransaction(const std::vector<uint8_t>& signedTx);
    uint64_t eth_estimateGas(const Bytes20& from, const Bytes20& to,
                              const std::vector<uint8_t>& data, uint64_t value = 0);
    MiningState getMiningState(const Bytes20& contract);
    Bytes32 getChallenge(const Bytes20& contract, const Bytes20& miner);
    Bytes32 getCurrentDifficulty(const Bytes20& contract);
private:
    std::string url_;
    std::mutex mtx_;
    void* curl_ = nullptr;
    static uint64_t parseHexU64(const std::string& hex);
    static std::string extractResult(const std::string& response);
};
