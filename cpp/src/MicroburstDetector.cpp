#include "MicroburstDetector.h"
#include <algorithm>
#include <iostream>

namespace tickshaper {

MicroburstDetector::MicroburstDetector(uint32_t threshold, uint32_t end_threshold, uint64_t min_duration) 
    : metrics_(nullptr), last_update_(std::chrono::steady_clock::now()),
      microburst_threshold_(threshold), microburst_end_threshold_(end_threshold),
      min_microburst_duration_ms_(min_duration) {
}

MicroburstDetector::~MicroburstDetector() = default;

void MicroburstDetector::Initialize(SystemMetrics* metrics) {
    metrics_ = metrics;
    
    // Initialize rate buckets
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    for (auto& bucket : rate_buckets_) {
        bucket.count.store(0);
        bucket.timestamp.store(timestamp);
    }
    
    last_update_ = now;
}

void MicroburstDetector::CheckMessage(const TickData& tick_data) {
    if (!metrics_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Update rate window
    UpdateRateWindow();
    
    // Add message to current bucket
    size_t bucket_index = (current_time_ms / BUCKET_SIZE_MS) % NUM_BUCKETS;
    auto& bucket = rate_buckets_[bucket_index];
    
    // Check if we need to reset this bucket (new time window)
    uint64_t bucket_time = (current_time_ms / BUCKET_SIZE_MS) * BUCKET_SIZE_MS;
    uint64_t expected_time = bucket.timestamp.load();
    
    if (bucket_time != expected_time) {
        // Reset bucket for new time window
        bucket.count.store(0);
        bucket.timestamp.store(bucket_time);
    }
    
    bucket.count.fetch_add(1);
    current_bucket_.store(bucket_index);
    
    // Calculate current rate and detect microbursts
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_).count() >= 10) {
        DetectMicroburst();
        last_update_ = now;
    }
}

void MicroburstDetector::UpdateRateWindow() {
    auto now = std::chrono::steady_clock::now();
    auto current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Calculate total messages in the last second
    uint32_t total_messages = 0;
    uint64_t window_start = current_time_ms - WINDOW_SIZE_MS;
    
    for (const auto& bucket : rate_buckets_) {
        uint64_t bucket_time = bucket.timestamp.load();
        if (bucket_time >= window_start) {
            total_messages += bucket.count.load();
        }
    }
    
    current_rate_.store(total_messages);
}

void MicroburstDetector::DetectMicroburst() {
    uint32_t current_rate = current_rate_.load();
    auto now = std::chrono::steady_clock::now();
    auto current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    bool was_in_microburst = in_microburst_.load();
    
    if (!was_in_microburst && current_rate > microburst_threshold_) {
        // Start of microburst
        in_microburst_.store(true);
        microburst_start_time_.store(current_time_ms);
        microburst_peak_rate_.store(current_rate);
        microburst_message_count_.store(current_rate);
        
        if (metrics_) {
            metrics_->microburst_detected.store(true);
        }
        
        std::cout << "Microburst detected! Rate: " << current_rate << " msg/s" << std::endl;
        
    } else if (was_in_microburst) {
        // Update peak rate
        uint32_t current_peak = microburst_peak_rate_.load();
        if (current_rate > current_peak) {
            microburst_peak_rate_.store(current_rate);
        }
        
        // Accumulate message count
        microburst_message_count_.fetch_add(current_rate / 100); // Approximate
        
        // Check for end of microburst
        if (current_rate < microburst_end_threshold_) {
            uint64_t start_time = microburst_start_time_.load();
            uint64_t duration = current_time_ms - start_time;
            
            if (duration >= min_microburst_duration_ms_) {
                // End of microburst - record event
                MicroburstEvent event;
                event.start_time = start_time;
                event.end_time = current_time_ms;
                event.peak_rate = microburst_peak_rate_.load();
                event.total_messages = microburst_message_count_.load();
                event.severity = CalculateSeverity(event.peak_rate);
                
                {
                    std::lock_guard<std::mutex> lock(events_mutex_);
                    recent_events_.push_back(event);
                    
                    // Keep only recent events
                    if (recent_events_.size() > MAX_EVENTS) {
                        recent_events_.erase(recent_events_.begin());
                    }
                }
                
                std::cout << "Microburst ended. Duration: " << duration << "ms, "
                          << "Peak: " << event.peak_rate << " msg/s, "
                          << "Severity: " << event.severity << std::endl;
            }
            
            in_microburst_.store(false);
            if (metrics_) {
                metrics_->microburst_detected.store(false);
            }
        }
    }
}

std::string MicroburstDetector::CalculateSeverity(uint32_t rate) {
    if (rate > 200000) {
        return "high";
    } else if (rate > 100000) {
        return "medium";
    } else {
        return "low";
    }
}

std::vector<MicroburstEvent> MicroburstDetector::GetRecentEvents() const {
    std::lock_guard<std::mutex> lock(events_mutex_);
    return recent_events_;
}

} // namespace tickshaper