#include "MessageProcessor.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <arpa/inet.h>

namespace tickshaper {

uint32_t SymbolManager::GetSymbolId(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = symbol_to_id_.find(symbol);
    if (it != symbol_to_id_.end()) {
        return it->second;
    }
    
    uint32_t id = next_id_.fetch_add(1);
    symbol_to_id_[symbol] = id;
    id_to_symbol_[id] = symbol;
    
    return id;
}

std::string SymbolManager::GetSymbol(uint32_t symbol_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = id_to_symbol_.find(symbol_id);
    if (it != id_to_symbol_.end()) {
        return it->second;
    }
    
    return "";
}

size_t SymbolManager::GetSymbolCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return symbol_to_id_.size();
}

MessageProcessor::MessageProcessor() 
    : shm_manager_(nullptr), metrics_(nullptr) {
}

MessageProcessor::~MessageProcessor() = default;

void MessageProcessor::Initialize(SharedMemoryManager* shm_manager, SystemMetrics* metrics) {
    shm_manager_ = shm_manager;
    metrics_ = metrics;
    
    std::cout << "Message processor initialized" << std::endl;
}

bool MessageProcessor::ProcessMessage(const RawMessage& raw_message, TickData& tick_data) {
    if (!shm_manager_ || !metrics_) {
        return false;
    }
    
    queue_depth_.fetch_add(1);
    
    bool processed = false;
    
    try {
        switch (raw_message.message_type) {
            case 'A': // Add Order - No MPID Attribution
            case 'F': // Add Order - MPID Attribution
                processed = ProcessAddOrder(raw_message, tick_data);
                if (processed) processed_add_orders_.fetch_add(1);
                break;
                
            case 'E': // Order Executed
                processed = ProcessOrderExecuted(raw_message, tick_data);
                if (processed) processed_executions_.fetch_add(1);
                break;
                
            case 'P': // Trade Message (Non-Cross)
            case 'Q': // Cross Trade Message
                processed = ProcessTrade(raw_message, tick_data);
                if (processed) processed_trades_.fetch_add(1);
                break;
                
            case 'X': // Order Cancel
            case 'D': // Order Delete
                processed = ProcessOrderCancel(raw_message, tick_data);
                if (processed) processed_cancels_.fetch_add(1);
                break;
                
            default:
                // Unknown message type, create basic tick data
                tick_data.timestamp = raw_message.timestamp;
                tick_data.symbol_id = 0;
                tick_data.price = 0;
                tick_data.size = 0;
                tick_data.side = 'U'; // Unknown
                tick_data.message_type = raw_message.message_type;
                processed = true;
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing message type " << static_cast<char>(raw_message.message_type) 
                  << ": " << e.what() << std::endl;
        processed = false;
    }
    
    queue_depth_.fetch_sub(1);
    return processed;
}

size_t MessageProcessor::GetActiveOrderCount() const {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    return active_orders_.size();
}

bool MessageProcessor::ProcessAddOrder(const RawMessage& raw_message, TickData& tick_data) {
    if (raw_message.data.size() < 36) {
        return false;
    }
    
    const uint8_t* data = raw_message.data.data();
    
    // Parse ITCH Add Order message
    uint16_t stock_locate = ExtractUint16(data);
    uint16_t tracking_number = ExtractUint16(data + 2);
    uint64_t order_reference = ExtractUint64(data + 10);
    char buy_sell_indicator = static_cast<char>(data[18]);
    uint32_t shares = ExtractUint32(data + 19);
    char stock_symbol[9] = {0};
    memcpy(stock_symbol, data + 23, 8);
    uint32_t price = ExtractUint32(data + 31);
    
    // Clean up symbol (remove padding)
    std::string symbol = ExtractSymbol(stock_symbol, 8);
    uint32_t symbol_id = symbol_manager_.GetSymbolId(symbol);
    
    // Store order in order book
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        active_orders_[order_reference] = {
            order_reference,
            ConvertPrice(price),
            shares,
            buy_sell_indicator,
            raw_message.timestamp,
            symbol
        };
    }
    
    // Create tick data
    tick_data.timestamp = raw_message.timestamp;
    tick_data.symbol_id = symbol_id;
    tick_data.price = ConvertPrice(price);
    tick_data.size = shares;
    tick_data.side = buy_sell_indicator;
    tick_data.message_type = raw_message.message_type;
    
    return true;
}

bool MessageProcessor::ProcessOrderExecuted(const RawMessage& raw_message, TickData& tick_data) {
    if (raw_message.data.size() < 30) {
        return false;
    }
    
    const uint8_t* data = raw_message.data.data();
    
    uint64_t order_reference = ExtractUint64(data + 10);
    uint32_t executed_shares = ExtractUint32(data + 18);
    uint64_t match_number = ExtractUint64(data + 22);
    
    // Find the original order
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = active_orders_.find(order_reference);
    if (it == active_orders_.end()) {
        // Order not found, create basic tick data
        tick_data.timestamp = raw_message.timestamp;
        tick_data.symbol_id = 0;
        tick_data.price = 0;
        tick_data.size = executed_shares;
        tick_data.side = 'U';
        tick_data.message_type = raw_message.message_type;
        return true;
    }
    
    const OrderBookEntry& order = it->second;
    
    // Create tick data for execution
    tick_data.timestamp = raw_message.timestamp;
    tick_data.symbol_id = symbol_manager_.GetSymbolId(order.symbol);
    tick_data.price = order.price;
    tick_data.size = executed_shares;
    tick_data.side = order.side;
    tick_data.message_type = raw_message.message_type;
    
    // Update order size
    it->second.size -= executed_shares;
    if (it->second.size == 0) {
        active_orders_.erase(it);
    }
    
    return true;
}

bool MessageProcessor::ProcessTrade(const RawMessage& raw_message, TickData& tick_data) {
    if (raw_message.data.size() < 43) {
        return false;
    }
    
    const uint8_t* data = raw_message.data.data();
    
    uint64_t order_reference = ExtractUint64(data + 10);
    char buy_sell_indicator = static_cast<char>(data[18]);
    uint32_t shares = ExtractUint32(data + 19);
    char stock_symbol[9] = {0};
    memcpy(stock_symbol, data + 23, 8);
    uint32_t price = ExtractUint32(data + 31);
    uint64_t match_number = ExtractUint64(data + 35);
    
    std::string symbol = ExtractSymbol(stock_symbol, 8);
    uint32_t symbol_id = symbol_manager_.GetSymbolId(symbol);
    
    // Create tick data for trade
    tick_data.timestamp = raw_message.timestamp;
    tick_data.symbol_id = symbol_id;
    tick_data.price = ConvertPrice(price);
    tick_data.size = shares;
    tick_data.side = buy_sell_indicator;
    tick_data.message_type = raw_message.message_type;
    
    return true;
}

bool MessageProcessor::ProcessOrderCancel(const RawMessage& raw_message, TickData& tick_data) {
    if (raw_message.data.size() < 22) {
        return false;
    }
    
    const uint8_t* data = raw_message.data.data();
    
    uint64_t order_reference = ExtractUint64(data + 10);
    uint32_t cancelled_shares = (raw_message.message_type == 'X' && raw_message.data.size() >= 22) ? 
                               ExtractUint32(data + 18) : 0;
    
    // Find and update the order
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = active_orders_.find(order_reference);
    if (it == active_orders_.end()) {
        // Order not found, create basic tick data
        tick_data.timestamp = raw_message.timestamp;
        tick_data.symbol_id = 0;
        tick_data.price = 0;
        tick_data.size = cancelled_shares;
        tick_data.side = 'U';
        tick_data.message_type = raw_message.message_type;
        return true;
    }
    
    const OrderBookEntry& order = it->second;
    
    // Create tick data for cancellation
    tick_data.timestamp = raw_message.timestamp;
    tick_data.symbol_id = symbol_manager_.GetSymbolId(order.symbol);
    tick_data.price = order.price;
    tick_data.size = (raw_message.message_type == 'D') ? order.size : cancelled_shares;
    tick_data.side = order.side;
    tick_data.message_type = raw_message.message_type;
    
    // Update or remove order
    if (raw_message.message_type == 'D') {
        // Delete entire order
        active_orders_.erase(it);
    } else {
        // Cancel partial shares
        it->second.size -= cancelled_shares;
        if (it->second.size == 0) {
            active_orders_.erase(it);
        }
    }
    
    return true;
}

uint32_t MessageProcessor::ConvertPrice(uint32_t itch_price) {
    // ITCH prices are in 1/10000 of a dollar
    // Convert to cents (1/100 of a dollar)
    return itch_price / 100;
}

std::string MessageProcessor::ExtractSymbol(const char* symbol_data, size_t length) {
    std::string symbol(symbol_data, length);
    
    // Remove trailing spaces
    symbol.erase(std::find_if(symbol.rbegin(), symbol.rend(), 
                             [](unsigned char ch) { return !std::isspace(ch); }).base(), 
                 symbol.end());
    
    return symbol;
}

uint64_t MessageProcessor::ExtractUint64(const uint8_t* data) {
    return be64toh(*reinterpret_cast<const uint64_t*>(data));
}

uint32_t MessageProcessor::ExtractUint32(const uint8_t* data) {
    return ntohl(*reinterpret_cast<const uint32_t*>(data));
}

uint16_t MessageProcessor::ExtractUint16(const uint8_t* data) {
    return ntohs(*reinterpret_cast<const uint16_t*>(data));
}

} // namespace tickshaper