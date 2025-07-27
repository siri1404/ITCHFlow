#include "TickShaper.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

using namespace tickshaper;

std::unique_ptr<TickShaper> g_tickshaper;

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_tickshaper) {
        g_tickshaper->Stop();
    }
    exit(0);
}

void PrintMetrics(const TickShaper& tickshaper) {
    const auto& metrics = tickshaper.GetMetrics();
    
    std::cout << "\n=== TickShaper Metrics ===" << std::endl;
    std::cout << "Messages Processed: " << metrics.messages_processed.load() << std::endl;
    std::cout << "Messages Throttled: " << metrics.messages_throttled.load() << std::endl;
    std::cout << "Current Throughput: " << metrics.current_throughput.load() << " msg/s" << std::endl;
    std::cout << "Queue Depth: " << metrics.queue_depth.load() << std::endl;
    std::cout << "CPU Usage: " << metrics.cpu_usage.load() << "%" << std::endl;
    std::cout << "Memory Usage: " << (metrics.memory_usage.load() / 1024 / 1024) << " MB" << std::endl;
    
    if (metrics.messages_processed.load() > 0) {
        double avg_latency_ms = static_cast<double>(metrics.total_latency_ns.load()) / 
                               metrics.messages_processed.load() / 1000000.0;
        std::cout << "Average Latency: " << avg_latency_ms << " ms" << std::endl;
    }
    
    if (metrics.microburst_detected.load()) {
        std::cout << "*** MICROBURST DETECTED ***" << std::endl;
    }
    
    std::cout << "=========================" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "TickShaper - Real-Time Market Data Throttler" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Install signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    // Create TickShaper instance
    g_tickshaper = std::make_unique<TickShaper>();
    
    // Initialize with configuration
    std::string config_file = "tickshaper.conf";
    if (argc > 1) {
        config_file = argv[1];
    }
    
    if (!g_tickshaper->Initialize(config_file)) {
        std::cerr << "Failed to initialize TickShaper" << std::endl;
        return 1;
    }
    
    // Start processing
    g_tickshaper->Start();
    
    // Metrics reporting loop
    std::thread metrics_thread([&]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            PrintMetrics(*g_tickshaper);
        }
    });
    
    // Interactive command loop
    std::string command;
    std::cout << "\nCommands: speed <multiplier>, throttle <rate>, reset, quit" << std::endl;
    std::cout << "> ";
    
    while (std::getline(std::cin, command)) {
        if (command == "quit" || command == "q") {
            break;
        } else if (command.substr(0, 5) == "speed") {
            try {
                double speed = std::stod(command.substr(6));
                g_tickshaper->SetReplaySpeed(speed);
            } catch (const std::exception& e) {
                std::cout << "Invalid speed value" << std::endl;
            }
        } else if (command.substr(0, 8) == "throttle") {
            try {
                uint32_t rate = std::stoul(command.substr(9));
                g_tickshaper->SetThrottleRate(rate);
            } catch (const std::exception& e) {
                std::cout << "Invalid throttle rate" << std::endl;
            }
        } else if (command == "reset") {
            g_tickshaper->ResetCounters();
            std::cout << "Counters reset" << std::endl;
        } else if (command == "metrics") {
            PrintMetrics(*g_tickshaper);
        } else if (!command.empty()) {
            std::cout << "Unknown command: " << command << std::endl;
        }
        
        std::cout << "> ";
    }
    
    // Cleanup
    g_tickshaper->Stop();
    g_tickshaper.reset();
    
    std::cout << "TickShaper shutdown complete" << std::endl;
    return 0;
}