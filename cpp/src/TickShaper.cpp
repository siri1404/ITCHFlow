#include "TickShaper.h"
#include "MessageProcessor.h"
#include "ITCHParser.h"
#include "ZMQPublisher.h"
#include "SharedMemoryManager.h"
#include "MicroburstDetector.h"
#include "ThrottleController.h"
#include <fstream>
#include <iostream>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sstream>

namespace tickshaper {

TickShaper::TickShaper() {
    processor_ = std::make_unique<MessageProcessor>();
    itch_parser_ = std::make_unique<ITCHParser>();
    publisher_ = std::make_unique<ZMQPublisher>();
    shm_manager_ = std::make_unique<SharedMemoryManager>();
    microburst_detector_ = std::make_unique<MicroburstDetector>();
    throttle_controller_ = std::make_unique<ThrottleController>();
}

TickShaper::~TickShaper() {
    Stop();
}

bool TickShaper::Initialize(const std::string& config_file) {
    try {
        if (!LoadConfiguration(config_file)) {
            std::cerr << "Failed to load configuration" << std::endl;
            return false;
        }
        
        // Initialize shared memory
        if (!shm_manager_->Initialize(shared_memory_size_)) {
            std::cerr << "Failed to initialize shared memory" << std::endl;
            return false;
        }
        
        // Initialize ZeroMQ publisher
        if (!publisher_->Initialize(zmq_endpoint_)) {
            std::cerr << "Failed to initialize ZMQ publisher" << std::endl;
            return false;
        }
        
        // Initialize ITCH parser
        if (!itch_parser_->Initialize(input_file_, symbols_file_)) {
            std::cerr << "Failed to initialize ITCH parser" << std::endl;
            return false;
        }
        
        // Initialize message processor
        processor_->Initialize(shm_manager_.get(), &metrics_);
        
        // Initialize microburst detector
        microburst_detector_->Initialize(&metrics_);
        
        // Initialize throttle controller
        throttle_controller_->Initialize(throttle_rate_.load());
        
        std::cout << "TickShaper initialized successfully" << std::endl;
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Input file: " << input_file_ << std::endl;
        std::cout << "  ZMQ endpoint: " << zmq_endpoint_ << std::endl;
        std::cout << "  Shared memory: " << (shared_memory_size_ / 1024 / 1024) << " MB" << std::endl;
        std::cout << "  Worker threads: " << worker_thread_count_ << std::endl;
        std::cout << "  CPU affinity: " << (enable_cpu_affinity_ ? "enabled" : "disabled") << std::endl;
        std::cout << "  Symbols file: " << (symbols_file_.empty() ? "none (using defaults)" : symbols_file_) << std::endl;
        std::cout << "  Microburst threshold: " << microburst_threshold_ << " msg/s" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void TickShaper::Start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    start_time_ = std::chrono::steady_clock::now();
    
    // Start worker threads
    for (int i = 0; i < worker_thread_count_; ++i) {
        worker_threads_.emplace_back([this, i]() {
            if (enable_cpu_affinity_) {
                SetupCPUAffinity(i);
            }
            ProcessingLoop();
        });
    }
    
    // Start metrics update thread
    metrics_thread_ = std::thread([this]() {
        MetricsUpdateLoop();
    });
    
    std::cout << "TickShaper started with " << worker_thread_count_ << " worker threads" << std::endl;
}

void TickShaper::Stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "Stopping TickShaper..." << std::endl;
    running_.store(false);
    
    // Join worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Join metrics thread
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }
    
    // Stop components
    publisher_->Stop();
    
    std::cout << "TickShaper stopped" << std::endl;
    
    // Print final statistics
    const auto& metrics = GetMetrics();
    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "  Messages processed: " << metrics.messages_processed.load() << std::endl;
    std::cout << "  Messages throttled: " << metrics.messages_throttled.load() << std::endl;
    std::cout << "  Uptime: " << metrics.uptime_seconds.load() << " seconds" << std::endl;
    
    if (metrics.messages_processed.load() > 0) {
        double avg_latency_us = static_cast<double>(metrics.total_latency_ns.load()) / 
                               metrics.messages_processed.load() / 1000.0;
        std::cout << "  Average latency: " << avg_latency_us << " μs" << std::endl;
    }
}

void TickShaper::SetReplaySpeed(double speed) {
    if (speed <= 0.0 || speed > 100.0) {
        std::cerr << "Invalid replay speed: " << speed << std::endl;
        return;
    }
    
    replay_speed_.store(speed);
    std::cout << "Replay speed set to " << speed << "x" << std::endl;
}

void TickShaper::SetThrottleRate(uint32_t messages_per_second) {
    if (messages_per_second == 0 || messages_per_second > 1000000) {
        std::cerr << "Invalid throttle rate: " << messages_per_second << std::endl;
        return;
    }
    
    throttle_rate_.store(messages_per_second);
    throttle_controller_->SetRate(messages_per_second);
    std::cout << "Throttle rate set to " << messages_per_second << " msg/s" << std::endl;
}

void TickShaper::ResetCounters() {
    metrics_.messages_processed.store(0);
    metrics_.messages_throttled.store(0);
    metrics_.total_latency_ns.store(0);
    metrics_.current_throughput.store(0);
    metrics_.queue_depth.store(0);
    metrics_.microburst_detected.store(false);
    
    start_time_ = std::chrono::steady_clock::now();
    
    std::cout << "Metrics counters reset" << std::endl;
}

void TickShaper::ProcessingLoop() {
    auto last_time = std::chrono::high_resolution_clock::now();
    uint64_t message_count = 0;
    
    while (running_.load()) {
        try {
            // Parse next ITCH message
            auto message_data = itch_parser_->GetNextMessage();
            if (!message_data) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Apply replay speed control
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                current_time - last_time).count();
            
            double target_delay = 1000.0 / replay_speed_.load(); // microseconds
            if (elapsed < target_delay) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(static_cast<int>(target_delay - elapsed)));
            }
            last_time = std::chrono::high_resolution_clock::now();
            
            // Check throttle
            if (!throttle_controller_->ShouldProcess()) {
                metrics_.messages_throttled.fetch_add(1);
                continue;
            }
            
            // Process message
            TickData tick_data;
            if (processor_->ProcessMessage(*message_data, tick_data)) {
                // Publish to ZeroMQ
                publisher_->Publish(tick_data);
                
                // Update metrics
                auto end_time = std::chrono::high_resolution_clock::now();
                auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time - start_time).count();
                
                metrics_.messages_processed.fetch_add(1);
                metrics_.total_latency_ns.fetch_add(latency_ns);
                
                // Check for microburst
                microburst_detector_->CheckMessage(tick_data);
                
                message_count++;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Processing error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    std::cout << "Worker thread processed " << message_count << " messages" << std::endl;
}

void TickShaper::MetricsUpdateLoop() {
    auto last_update = std::chrono::steady_clock::now();
    uint64_t last_message_count = 0;
    
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count();
        
        if (elapsed >= 1) {
            uint64_t current_messages = metrics_.messages_processed.load();
            uint32_t throughput = static_cast<uint32_t>(
                (current_messages - last_message_count) / elapsed);
            
            metrics_.current_throughput.store(throughput);
            metrics_.queue_depth.store(processor_->GetQueueDepth());
            
            // Update uptime
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                now - start_time_).count();
            metrics_.uptime_seconds.store(uptime);
            
            // Update CPU and memory usage
            UpdateSystemMetrics();
            
            last_message_count = current_messages;
            last_update = now;
        }
    }
}

bool TickShaper::LoadConfiguration(const std::string& config_file) {
    // Default configuration
    input_file_ = "data/sample.itch";
    symbols_file_ = "";
    zmq_endpoint_ = "tcp://*:5555";
    shared_memory_size_ = 1024 * 1024 * 1024; // 1GB
    worker_thread_count_ = std::thread::hardware_concurrency();
    enable_cpu_affinity_ = true;
    microburst_threshold_ = 50000;
    log_level_ = "INFO";
    enable_monitoring_ = true;
    monitoring_interval_ = 1;
    
    // Load from file if exists
    std::ifstream file(config_file);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Simple key=value parser
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                if (key == "input_file") input_file_ = value;
                else if (key == "symbols_file") symbols_file_ = value;
                else if (key == "zmq_endpoint") zmq_endpoint_ = value;
                else if (key == "shared_memory_size") shared_memory_size_ = std::stoull(value);
                else if (key == "worker_threads") {
                    int threads = std::stoi(value);
                    worker_thread_count_ = (threads <= 0) ? std::thread::hardware_concurrency() : threads;
                }
                else if (key == "cpu_affinity") enable_cpu_affinity_ = (value == "true");
                else if (key == "default_throttle_rate") throttle_rate_.store(std::stoul(value));
                else if (key == "default_replay_speed") replay_speed_.store(std::stod(value));
                else if (key == "microburst_threshold") microburst_threshold_ = std::stoul(value);
                else if (key == "log_level") log_level_ = value;
                else if (key == "enable_monitoring") enable_monitoring_ = (value == "true");
                else if (key == "monitoring_interval") monitoring_interval_ = std::stoi(value);
            }
        }
        std::cout << "Configuration loaded from " << config_file << std::endl;
    } else {
        std::cout << "Using default configuration (config file not found: " << config_file << ")" << std::endl;
    }
    
    return true;
}

void TickShaper::UpdateSystemMetrics() {
    // Get CPU usage
    static auto last_cpu_time = std::chrono::steady_clock::now();
    static long last_user_time = 0;
    static long last_sys_time = 0;
    
    auto current_time = std::chrono::steady_clock::now();
    
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        long user_time = usage.ru_utime.tv_sec * 1000000 + usage.ru_utime.tv_usec;
        long sys_time = usage.ru_stime.tv_sec * 1000000 + usage.ru_stime.tv_usec;
        
        auto wall_time = std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - last_cpu_time).count();
        
        if (wall_time > 0 && (last_user_time > 0 || last_sys_time > 0)) {
            long cpu_time_diff = (user_time - last_user_time) + (sys_time - last_sys_time);
            double cpu_percent = (cpu_time_diff * 100.0) / wall_time;
            metrics_.cpu_usage.store(std::min(cpu_percent, 100.0));
        }
        
        // Memory usage in bytes (RSS)
        metrics_.memory_usage.store(usage.ru_maxrss * 1024);
        
        last_user_time = user_time;
        last_sys_time = sys_time;
    }
    
    last_cpu_time = current_time;
}

void TickShaper::SetupCPUAffinity(int thread_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    int cpu_count = std::thread::hardware_concurrency();
    int target_cpu = thread_id % cpu_count;
    
    CPU_SET(target_cpu, &cpuset);
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0) {
        std::cout << "Thread " << thread_id << " bound to CPU " << target_cpu << std::endl;
    } else {
        std::cerr << "Failed to set CPU affinity for thread " << thread_id << std::endl;
    }
}

} // namespace tickshaper