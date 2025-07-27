#pragma once

#include "TickShaper.h"
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>

namespace tickshaper {

struct MicroburstEvent {
    uint64_t start_time;
    uint64_t end_time;
    uint32_t peak_rate;
    uint32_t total_messages;
    std::string severity;
};

class MicroburstDetector {
public:
    MicroburstDetector(uint32_t threshold = 50000, uint32_t end_threshold = 30000, uint64_t min_duration = 100);
    ~MicroburstDetector();
    
    void Initialize(SystemMetrics* metrics);
    void CheckMessage(const TickData& tick_data);
    
    std::vector<MicroburstEvent> GetRecentEvents() const;
    bool IsCurrentlyInMicroburst() const { return in_microburst_.load(); }
    
private:
    void UpdateRateWindow();
    void DetectMicroburst();
    std::string CalculateSeverity(uint32_t rate);
    
    SystemMetrics* metrics_;
    
    // Rate calculation window
    static constexpr size_t WINDOW_SIZE_MS = 1000;
    static constexpr size_t BUCKET_SIZE_MS = 10;
    static constexpr size_t NUM_BUCKETS = WINDOW_SIZE_MS / BUCKET_SIZE_MS;
    
    struct RateBucket {
        std::atomic<uint32_t> count{0};
        std::atomic<uint64_t> timestamp{0};
    };
    
    std::array<RateBucket, NUM_BUCKETS> rate_buckets_;
    std::atomic<size_t> current_bucket_{0};
    std::atomic<uint32_t> current_rate_{0};
    
    // Microburst detection
    std::atomic<bool> in_microburst_{false};
    std::atomic<uint64_t> microburst_start_time_{0};
    std::atomic<uint32_t> microburst_peak_rate_{0};
    std::atomic<uint32_t> microburst_message_count_{0};
    
    // Thresholds
    uint32_t microburst_threshold_;
    uint32_t microburst_end_threshold_;
    uint64_t min_microburst_duration_ms_;
    
    // Event history
    mutable std::mutex events_mutex_;
    std::vector<MicroburstEvent> recent_events_;
    static constexpr size_t MAX_EVENTS = 100;
    
    std::chrono::steady_clock::time_point last_update_;
};

} // namespace tickshaper