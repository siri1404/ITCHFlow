#include "ITCHParser.h"
#include <iostream>
#include <cstring>
#include <random>
#include <arpa/inet.h>

namespace tickshaper {

ITCHParser::ITCHParser() 
    : total_messages_(0), current_position_(0), file_size_(0), 
      initialized_(false), using_sample_data_(false), sample_timestamp_(0), 
      sample_order_ref_(1000000), min_price_(1000), max_price_(100000),
      min_size_(100), max_size_(10000), message_interval_ns_(1000000) {
}

ITCHParser::~ITCHParser() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool ITCHParser::Initialize(const std::string& filename, const std::string& symbols_file) {
    filename_ = filename;
    file_.open(filename, std::ios::binary);
    
    if (!file_.is_open()) {
        std::cout << "ITCH file not found: " << filename << ", creating sample data..." << std::endl;
        if (!CreateSampleData(symbols_file)) {
            std::cerr << "Failed to create sample data" << std::endl;
            return false;
        }
        using_sample_data_ = true;
        initialized_ = true;
        return true;
    }
    
    // Get file size and estimate message count
    file_.seekg(0, std::ios::end);
    file_size_ = file_.tellg();
    file_.seekg(0, std::ios::beg);
    
    // Rough estimate: average message size ~50 bytes
    total_messages_ = file_size_ / 50;
    current_position_ = 0;
    initialized_ = true;
    
    std::cout << "ITCH Parser initialized. File size: " << file_size_ 
              << " bytes, estimated messages: " << total_messages_ << std::endl;
    
    return true;
}

std::unique_ptr<RawMessage> ITCHParser::GetNextMessage() {
    if (!initialized_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(file_mutex_);
    
    if (using_sample_data_) {
        // Generate synthetic ITCH message
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> symbol_dist(0, symbols_.size() - 1);
        static std::uniform_int_distribution<> price_dist(min_price_, max_price_);
        static std::uniform_int_distribution<> size_dist(min_size_, max_size_);
        static std::uniform_int_distribution<> side_dist(0, 1);
        static std::uniform_int_distribution<> msg_type_dist(0, 2);
        
        // Create synthetic Add Order message
        std::vector<uint8_t> message_data(36);
        uint8_t* data = message_data.data();
        
        // Stock locate and tracking number
        *reinterpret_cast<uint16_t*>(data) = htons(1);
        *reinterpret_cast<uint16_t*>(data + 2) = htons(current_position_ & 0xFFFF);
        
        // Timestamp (6 bytes, nanoseconds since midnight)
        sample_timestamp_ += message_interval_ns_ + (gen() % (message_interval_ns_ * 10));
        uint64_t ts = sample_timestamp_;
        data[4] = (ts >> 40) & 0xFF;
        data[5] = (ts >> 32) & 0xFF;
        data[6] = (ts >> 24) & 0xFF;
        data[7] = (ts >> 16) & 0xFF;
        data[8] = (ts >> 8) & 0xFF;
        data[9] = ts & 0xFF;
        
        // Order reference
        *reinterpret_cast<uint64_t*>(data + 10) = htobe64(sample_order_ref_++);
        
        // Buy/sell indicator
        data[18] = side_dist(gen) ? 'B' : 'S';
        
        // Shares
        *reinterpret_cast<uint32_t*>(data + 19) = htonl(size_dist(gen));
        
        // Stock symbol (8 bytes, space padded)
        const std::string& symbol = symbols_[symbol_dist(gen)];
        memset(data + 23, ' ', 8);
        memcpy(data + 23, symbol.c_str(), std::min(symbol.length(), size_t(8)));
        
        // Price
        *reinterpret_cast<uint32_t*>(data + 31) = htonl(price_dist(gen));
        
        current_position_++;
        
        // Vary message types
        uint8_t msg_types[] = {'A', 'E', 'P'};
        uint8_t msg_type = msg_types[msg_type_dist(gen)];
        
        return std::make_unique<RawMessage>(msg_type, sample_timestamp_, 
                                           message_data.data(), message_data.size());
    }
    
    // Read from actual ITCH file
    ITCHMessageHeader header;
    if (!ReadMessageHeader(header)) {
        return nullptr;
    }
    
    std::vector<uint8_t> message_data;
    if (!ReadMessageData(header.message_type, header.length, message_data)) {
        return nullptr;
    }
    
    uint64_t timestamp = ExtractTimestamp(header.message_type, message_data);
    current_position_++;
    
    return std::make_unique<RawMessage>(header.message_type, timestamp, 
                                       message_data.data(), message_data.size());
}

void ITCHParser::Reset() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    
    if (using_sample_data_) {
        current_position_ = 0;
        sample_timestamp_ = 0;
        sample_order_ref_ = 1000000;
    } else if (file_.is_open()) {
        file_.clear();
        file_.seekg(0, std::ios::beg);
        current_position_ = 0;
    }
}

bool ITCHParser::ReadMessageHeader(ITCHMessageHeader& header) {
    file_.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (file_.gcount() != sizeof(header)) {
        if (file_.eof()) {
            // End of file reached, reset to beginning for continuous replay
            Reset();
            return ReadMessageHeader(header);
        }
        return false;
    }
    
    // Convert from network byte order if necessary
    header.length = ntohs(header.length);
    
    return true;
}

bool ITCHParser::ReadMessageData(uint8_t message_type, uint16_t length, std::vector<uint8_t>& data) {
    // Length includes the message type byte, so subtract 1
    size_t data_length = length - 1;
    data.resize(data_length);
    
    file_.read(reinterpret_cast<char*>(data.data()), data_length);
    
    if (file_.gcount() != static_cast<std::streamsize>(data_length)) {
        return false;
    }
    
    return true;
}

uint64_t ITCHParser::ExtractTimestamp(uint8_t message_type, const std::vector<uint8_t>& data) {
    // ITCH timestamp is typically at offset 4 (after stock_locate and tracking_number)
    // and is 6 bytes in nanoseconds since midnight
    
    if (data.size() < 10) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
    
    uint64_t timestamp = 0;
    
    switch (message_type) {
        case 'A': // Add Order
        case 'F': // Add Order with MPID
        case 'E': // Order Executed
        case 'C': // Order Executed with Price
        case 'X': // Order Cancel
        case 'D': // Order Delete
        case 'U': // Order Replace
        case 'P': // Trade Message
        case 'Q': // Cross Trade Message
        case 'B': // Broken Trade Message
        {
            // Extract 6-byte timestamp at offset 4
            const uint8_t* ts_ptr = data.data() + 4;
            timestamp = (static_cast<uint64_t>(ts_ptr[0]) << 40) |
                       (static_cast<uint64_t>(ts_ptr[1]) << 32) |
                       (static_cast<uint64_t>(ts_ptr[2]) << 24) |
                       (static_cast<uint64_t>(ts_ptr[3]) << 16) |
                       (static_cast<uint64_t>(ts_ptr[4]) << 8) |
                       static_cast<uint64_t>(ts_ptr[5]);
            break;
        }
        default:
            // Use current system time for unknown message types
            timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            break;
    }
    
    return timestamp;
}

bool ITCHParser::LoadSymbolsFromFile(const std::string& symbols_file) {
    std::ifstream file(symbols_file);
    if (!file.is_open()) {
        return false;
    }
    
    symbols_.clear();
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse symbol configuration: SYMBOL,min_price,max_price,min_size,max_size
        std::istringstream iss(line);
        std::string symbol;
        if (std::getline(iss, symbol, ',')) {
            symbols_.push_back(symbol);
        }
    }
    
    return !symbols_.empty();
}

bool ITCHParser::CreateSampleData(const std::string& symbols_file) {
    // Try to load symbols from file first
    if (!symbols_file.empty() && LoadSymbolsFromFile(symbols_file)) {
        std::cout << "Loaded " << symbols_.size() << " symbols from " << symbols_file << std::endl;
    } else {
        // Fallback to default symbols if no file provided or loading failed
        symbols_ = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA", "META", "NVDA", "NFLX"};
        std::cout << "Using default symbols" << std::endl;
    }
    
    // Initialize sample data generation
    auto now = std::chrono::high_resolution_clock::now();
    auto midnight = std::chrono::time_point_cast<std::chrono::days>(now);
    sample_timestamp_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now - midnight).count();
    
    total_messages_ = 1000000; // Simulate 1M messages
    current_position_ = 0;
    
    std::cout << "Sample data generator initialized with " << symbols_.size() 
              << " symbols" << std::endl;
    
    return true;
}

} // namespace tickshaper