#include "ThrottleController.h"
#include <algorithm>

namespace tickshaper {

ThrottleController::ThrottleController() 
    : last_reset_(std::chrono::steady_clock::now()),
      last_process_time_(std::chrono::steady_clock::now()) {
}

ThrottleController::~ThrottleController() = default;

void ThrottleController::Initialize(uint32_t messages_per_second) {
    SetRate(messages_per_second);
    tokens_.store(static_cast<double>(messages_per_second));
}

void ThrottleController::SetRate(uint32_t messages_per_second) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    target_rate_.store(messages_per_second);
    token_rate_.store(static_cast<double>(messages_per_second));
    
    // Reset token bucket
    tokens_.store(std::min(static_cast<double>(messages_per_second), MAX_TOKENS));
}

bool ThrottleController::ShouldProcess() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Add tokens based on elapsed time
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - last_process_time_).count();
    
    if (elapsed > 0) {
        double tokens_to_add = (token_rate_.load() * elapsed) / 1000000.0;
        double current_tokens = tokens_.load();
        tokens_.store(std::min(current_tokens + tokens_to_add, MAX_TOKENS));
        
        last_process_time_ = now;
    }
    
    // Check if we have enough tokens
    double current_tokens = tokens_.load();
    if (current_tokens >= TOKENS_PER_MESSAGE) {
        tokens_.store(current_tokens - TOKENS_PER_MESSAGE);
        processed_count_.fetch_add(1);
        current_count_.fetch_add(1);
        
        // Reset counters every second
        auto reset_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_reset_).count();
        if (reset_elapsed >= 1) {
            ResetCounters();
        }
        
        return true;
    } else {
        throttled_count_.fetch_add(1);
        return false;
    }
}

void ThrottleController::ResetCounters() {
    current_count_.store(0);
    last_reset_ = std::chrono::steady_clock::now();
}

} // namespace tickshaper