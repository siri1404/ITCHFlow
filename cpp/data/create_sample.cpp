#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cstring>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>

// Simple utility to create sample ITCH data for testing
struct ITCHAddOrder {
    uint16_t length;
    uint8_t message_type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    uint64_t order_reference;
    char buy_sell_indicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
} __attribute__((packed));

struct SymbolConfig {
    std::string symbol;
    uint32_t min_price;
    uint32_t max_price;
    uint32_t min_size;
    uint32_t max_size;
};

std::vector<SymbolConfig> LoadSymbols(const std::string& symbols_file) {
    std::vector<SymbolConfig> symbols;
    std::ifstream file(symbols_file);
    
    if (!file.is_open()) {
        // Default symbols if file not found
        symbols = {
            {"AAPL", 1500000, 2000000, 100, 5000},
            {"MSFT", 3000000, 4000000, 100, 3000},
            {"GOOGL", 2500000, 3500000, 50, 2000},
            {"AMZN", 3200000, 3800000, 100, 2500},
            {"TSLA", 2000000, 3000000, 100, 4000}
        };
        return symbols;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string token;
        SymbolConfig config;
        
        if (std::getline(iss, token, ',')) config.symbol = token;
        if (std::getline(iss, token, ',')) config.min_price = std::stoul(token);
        if (std::getline(iss, token, ',')) config.max_price = std::stoul(token);
        if (std::getline(iss, token, ',')) config.min_size = std::stoul(token);
        if (std::getline(iss, token, ',')) config.max_size = std::stoul(token);
        
        symbols.push_back(config);
    }
    
    return symbols;
}

int main(int argc, char* argv[]) {
    std::string symbols_file = "../config/symbols.txt";
    std::string output_file = "sample.itch";
    int num_messages = 100000;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--symbols" && i + 1 < argc) {
            symbols_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--count" && i + 1 < argc) {
            num_messages = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --symbols <file>  Symbols configuration file\n"
                      << "  --output <file>   Output ITCH file\n"
                      << "  --count <num>     Number of messages to generate\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }
    
    auto symbols = LoadSymbols(symbols_file);
    if (symbols.empty()) {
        std::cerr << "No symbols loaded!" << std::endl;
        return 1;
    }
    
    std::ofstream file("sample.itch", std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create sample.itch" << std::endl;
        return 1;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> side_dist(0, 1);
    std::uniform_int_distribution<> symbol_dist(0, symbols.size() - 1);
    
    uint64_t timestamp = 34200000000000ULL; // 9:30 AM in nanoseconds
    uint64_t order_ref = 1000000;
    
    // Generate 100,000 sample messages
    for (int i = 0; i < num_messages; ++i) {
        const auto& symbol_config = symbols[symbol_dist(gen)];
        std::uniform_int_distribution<> price_dist(symbol_config.min_price, symbol_config.max_price);
        std::uniform_int_distribution<> size_dist(symbol_config.min_size, symbol_config.max_size);
        
        ITCHAddOrder order;
        order.length = htons(sizeof(ITCHAddOrder) - 2); // Exclude length field
        order.message_type = 'A';
        order.stock_locate = htons(1);
        order.tracking_number = htons(i & 0xFFFF);
        
        // Convert timestamp to big-endian 6-byte format
        timestamp += 1000000 + (gen() % 10000000); // Add 1-10ms
        uint64_t ts_be = htobe64(timestamp);
        memcpy(&order.timestamp, reinterpret_cast<char*>(&ts_be) + 2, 6);
        
        order.order_reference = htobe64(order_ref++);
        order.buy_sell_indicator = side_dist(gen) ? 'B' : 'S';
        order.shares = htonl(size_dist(gen));
        
        // Pad symbol to 8 characters
        std::string padded_symbol = symbol_config.symbol;
        padded_symbol.resize(8, ' ');
        memcpy(order.stock, padded_symbol.c_str(), 8);
        
        order.price = htonl(price_dist(gen));
        
        file.write(reinterpret_cast<const char*>(&order), sizeof(order));
    }
    
    file.close();
    std::cout << "Created " << output_file << " with " << num_messages 
              << " messages using " << symbols.size() << " symbols" << std::endl;
    return 0;
}