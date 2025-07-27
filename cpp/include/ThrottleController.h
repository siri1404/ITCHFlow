#pragma once

#include <atomic>
#include <chrono>
#include <thread>

namespace tickshaper {

class ThrottleController {
public:
    ThrottleController();
    ~ThrottleController();
    
    void Initialize(uint32_t messages_per_second);
    void SetRate(uint32_t messages_per_second);
    bool ShouldProcess();
    
    uint32_t GetCurrentRate() const { return target_rate_.load(); }
    uint64_t GetProcessedCount() const { return processed_count_.load(); }
    uint64_t GetThrottledCount() const { return throttled_count_.load(); }
    
private:
    void ResetCounters();
    
    std::atomic<uint32_t> target_rate_{100000};
    std::atomic<uint32_t> current_count_{0};
    std::atomic<uint64_t> processed_count_{0};
    std::atomic<uint64_t> throttled_count_{0};
    
    std::chrono::steady_clock::time_point last_reset_;
    std::chrono::steady_clock::time_point last_process_time_;
    
    // Token bucket algorithm parameters
    std::atomic<double> tokens_{0.0};
    std::atomic<double> token_rate_{100000.0}; // tokens per second
    static constexpr double MAX_TOKENS = 200000.0;
    static constexpr double TOKENS_PER_MESSAGE = 1.0;
    
    mutable std::mutex mutex_;
};

} // namespace tickshaper