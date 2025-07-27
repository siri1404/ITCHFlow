#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <vector>
#include <cstdint>
#include <mutex>

namespace tickshaper {

#pragma pack(push, 1)
struct ITCHMessageHeader {
    uint16_t length;
    uint8_t message_type;
};

struct ITCHAddOrder {
    ITCHMessageHeader header;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    uint64_t order_reference;
    char buy_sell_indicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
};

struct ITCHOrderExecuted {
    ITCHMessageHeader header;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    uint64_t order_reference;
    uint32_t executed_shares;
    uint64_t match_number;
};

struct ITCHTrade {
    ITCHMessageHeader header;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    uint64_t order_reference;
    char buy_sell_indicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    uint64_t match_number;
};
#pragma pack(pop)

struct RawMessage {
    uint8_t message_type;
    uint64_t timestamp;
    std::vector<uint8_t> data;
    
    RawMessage(uint8_t type, uint64_t ts, const void* msg_data, size_t size)
        : message_type(type), timestamp(ts), data(static_cast<const uint8_t*>(msg_data), 
          static_cast<const uint8_t*>(msg_data) + size) {}
};

class ITCHParser {
public:
    ITCHParser();
    ~ITCHParser();
    
    bool Initialize(const std::string& filename, const std::string& symbols_file = "");
    std::unique_ptr<RawMessage> GetNextMessage();
    void Reset();
    
    uint64_t GetTotalMessages() const { return total_messages_; }
    uint64_t GetCurrentPosition() const { return current_position_; }
    size_t GetFileSize() const { return file_size_; }
    
private:
    bool ReadMessageHeader(ITCHMessageHeader& header);
    bool ReadMessageData(uint8_t message_type, uint16_t length, std::vector<uint8_t>& data);
    uint64_t ExtractTimestamp(uint8_t message_type, const std::vector<uint8_t>& data);
    bool LoadSymbolsFromFile(const std::string& symbols_file);
    bool CreateSampleData(const std::string& symbols_file);
    
    std::ifstream file_;
    std::string filename_;
    uint64_t total_messages_;
    uint64_t current_position_;
    size_t file_size_;
    bool initialized_;
    bool using_sample_data_;
    
    // Sample data generation
    std::vector<std::string> symbols_;
    uint64_t sample_timestamp_;
    uint64_t sample_order_ref_;
    uint32_t min_price_;
    uint32_t max_price_;
    uint32_t min_size_;
    uint32_t max_size_;
    uint64_t message_interval_ns_;
    
    mutable std::mutex file_mutex_;
};

} // namespace tickshaper