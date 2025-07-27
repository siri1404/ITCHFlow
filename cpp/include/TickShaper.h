#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <functional>
#include <string>

namespace tickshaper {

class MessageProcessor;
class ITCHParser;
class ZMQPublisher;
class SharedMemoryManager;
class MicroburstDetector;
class ThrottleController;

struct TickData {
    uint64_t timestamp;
    uint32_t symbol_id;
    uint64_t price;
    uint32_t size;
    char side;
    uint8_t message_type;
    
    TickData() = default;
    TickData(uint64_t ts, uint32_t sym, uint64_t p, uint32_t sz, char s, uint8_t mt)
        : timestamp(ts), symbol_id(sym), price(p), size(sz), side(s), message_type(mt) {}
};

struct SystemMetrics {
    std::atomic<uint64_t> messages_processed{0};
    std::atomic<uint64_t> messages_throttled{0};
    std::atomic<uint64_t> total_latency_ns{0};
    std::atomic<uint32_t> current_throughput{0};
    std::atomic<uint32_t> queue_depth{0};
    std::atomic<bool> microburst_detected{false};
    std::atomic<double> cpu_usage{0.0};
    std::atomic<uint64_t> memory_usage{0};
    std::atomic<uint64_t> uptime_seconds{0};
};

class TickShaper {
public:
    TickShaper();
    ~TickShaper();
    
    bool Initialize(const std::string& config_file);
    void Start();
    void Stop();
    
    void SetReplaySpeed(double speed);
    void SetThrottleRate(uint32_t messages_per_second);
    void ResetCounters();
    
    const SystemMetrics& GetMetrics() const { return metrics_; }
    bool IsRunning() const { return running_.load(); }
    
private:
    void ProcessingLoop();
    void MetricsUpdateLoop();
    bool LoadConfiguration(const std::string& config_file);
    void UpdateSystemMetrics();
    void SetupCPUAffinity(int thread_id);
    
    std::unique_ptr<MessageProcessor> processor_;
    std::unique_ptr<ITCHParser> itch_parser_;
    std::unique_ptr<ZMQPublisher> publisher_;
    std::unique_ptr<SharedMemoryManager> shm_manager_;
    std::unique_ptr<MicroburstDetector> microburst_detector_;
    std::unique_ptr<ThrottleController> throttle_controller_;
    
    SystemMetrics metrics_;
    std::atomic<bool> running_{false};
    std::atomic<double> replay_speed_{1.0};
    std::atomic<uint32_t> throttle_rate_{100000};
    
    std::vector<std::thread> worker_threads_;
    std::thread metrics_thread_;
    
    // Configuration
    std::string input_file_;
    std::string symbols_file_;
    std::string zmq_endpoint_;
    size_t shared_memory_size_;
    int worker_thread_count_;
    bool enable_cpu_affinity_;
    uint32_t microburst_threshold_;
    std::string log_level_;
    bool enable_monitoring_;
    int monitoring_interval_;
    
    // Timing
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace tickshaper