#pragma once

#include "TickShaper.h"
#include "ITCHParser.h"
#include "SharedMemoryManager.h"
#include <unordered_map>
#include <string>
#include <atomic>
#include <mutex>

namespace tickshaper {

struct OrderBookEntry {
    uint64_t order_id;
    uint32_t price;
    uint32_t size;
    char side;
    uint64_t timestamp;
    std::string symbol;
};

class SymbolManager {
public:
    uint32_t GetSymbolId(const std::string& symbol);
    std::string GetSymbol(uint32_t symbol_id);
    size_t GetSymbolCount() const;
    
private:
    std::unordered_map<std::string, uint32_t> symbol_to_id_;
    std::unordered_map<uint32_t, std::string> id_to_symbol_;
    std::atomic<uint32_t> next_id_{1};
    mutable std::mutex mutex_;
};

class MessageProcessor {
public:
    MessageProcessor();
    ~MessageProcessor();
    
    void Initialize(SharedMemoryManager* shm_manager, SystemMetrics* metrics);
    bool ProcessMessage(const RawMessage& raw_message, TickData& tick_data);
    
    uint32_t GetQueueDepth() const { return queue_depth_.load(); }
    size_t GetActiveOrderCount() const;
    
private:
    bool ProcessAddOrder(const RawMessage& raw_message, TickData& tick_data);
    bool ProcessOrderExecuted(const RawMessage& raw_message, TickData& tick_data);
    bool ProcessTrade(const RawMessage& raw_message, TickData& tick_data);
    bool ProcessOrderCancel(const RawMessage& raw_message, TickData& tick_data);
    
    uint32_t ConvertPrice(uint32_t itch_price);
    std::string ExtractSymbol(const char* symbol_data, size_t length);
    uint64_t ExtractUint64(const uint8_t* data);
    uint32_t ExtractUint32(const uint8_t* data);
    uint16_t ExtractUint16(const uint8_t* data);
    
    SharedMemoryManager* shm_manager_;
    SystemMetrics* metrics_;
    SymbolManager symbol_manager_;
    
    std::atomic<uint32_t> queue_depth_{0};
    
    // Order book tracking
    std::unordered_map<uint64_t, OrderBookEntry> active_orders_;
    mutable std::mutex orders_mutex_;
    
    // Statistics
    std::atomic<uint64_t> processed_add_orders_{0};
    std::atomic<uint64_t> processed_executions_{0};
    std::atomic<uint64_t> processed_trades_{0};
    std::atomic<uint64_t> processed_cancels_{0};
};

} // namespace tickshaper