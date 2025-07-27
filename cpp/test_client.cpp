#include <zmq.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <signal.h>
#include <json/json.h>

// Simple ZeroMQ client to test TickShaper output
class TickShaperClient {
public:
    TickShaperClient() : context_(1), subscriber_(context_, ZMQ_SUB), running_(true) {
        // Connect to TickShaper
        subscriber_.connect("tcp://localhost:5555");
        
        // Subscribe to all messages
        subscriber_.setsockopt(ZMQ_SUBSCRIBE, "", 0);
        
        // Set receive timeout
        int timeout = 1000; // 1 second
        subscriber_.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        
        std::cout << "Connected to TickShaper on tcp://localhost:5555" << std::endl;
    }
    
    void Run() {
        auto start_time = std::chrono::steady_clock::now();
        uint64_t message_count = 0;
        uint64_t last_count = 0;
        auto last_time = start_time;
        
        while (running_.load()) {
            zmq::message_t message;
            
            try {
                auto result = subscriber_.recv(message, zmq::recv_flags::none);
                if (result) {
                    message_count++;
                    
                    // Parse and display message
                    std::string data(static_cast<char*>(message.data()), message.size());
                    ProcessMessage(data);
                    
                    // Print statistics every 5 seconds
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_time).count();
                    
                    if (elapsed >= 5) {
                        uint64_t messages_in_period = message_count - last_count;
                        double rate = static_cast<double>(messages_in_period) / elapsed;
                        
                        std::cout << "\n=== Statistics ===" << std::endl;
                        std::cout << "Total messages: " << message_count << std::endl;
                        std::cout << "Rate: " << rate << " msg/s" << std::endl;
                        std::cout << "=================" << std::endl;
                        
                        last_count = message_count;
                        last_time = now;
                    }
                }
            } catch (const zmq::error_t& e) {
                if (e.num() == EAGAIN) {
                    // Timeout - continue
                    continue;
                } else {
                    std::cerr << "ZMQ error: " << e.what() << std::endl;
                    break;
                }
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
        
        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << "Total messages: " << message_count << std::endl;
        std::cout << "Total time: " << total_time << " seconds" << std::endl;
        if (total_time > 0) {
            std::cout << "Average rate: " << (message_count / total_time) << " msg/s" << std::endl;
        }
    }
    
    void Stop() {
        running_.store(false);
    }
    
private:
    void ProcessMessage(const std::string& data) {
        // Try to parse as JSON
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(data, root)) {
            // Display parsed message
            static int display_count = 0;
            if (display_count++ % 1000 == 0) { // Display every 1000th message
                std::cout << "Tick: Symbol=" << root["symbol_id"].asUInt()
                          << " Price=" << root["price"].asUInt()
                          << " Size=" << root["size"].asUInt()
                          << " Side=" << root["side"].asString()
                          << " Type=" << root["message_type"].asString() << std::endl;
            }
        } else {
            // Raw message
            static int raw_count = 0;
            if (raw_count++ % 1000 == 0) {
                std::cout << "Raw message (" << data.size() << " bytes): " 
                          << data.substr(0, 50) << "..." << std::endl;
            }
        }
    }
    
    zmq::context_t context_;
    zmq::socket_t subscriber_;
    std::atomic<bool> running_;
};

std::unique_ptr<TickShaperClient> g_client;

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_client) {
        g_client->Stop();
    }
}

int main() {
    std::cout << "TickShaper Test Client" << std::endl;
    std::cout << "=====================" << std::endl;
    
    // Install signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    try {
        g_client = std::make_unique<TickShaperClient>();
        g_client->Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}