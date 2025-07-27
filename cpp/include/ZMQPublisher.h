#pragma once

#include "TickShaper.h"
#include <zmq.hpp>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace tickshaper {

class ZMQPublisher {
public:
    ZMQPublisher();
    ~ZMQPublisher();
    
    bool Initialize(const std::string& endpoint);
    void Publish(const TickData& tick_data);
    void Stop();
    
    uint64_t GetPublishedCount() const { return published_count_.load(); }
    
private:
    void PublishingLoop();
    std::string SerializeTickData(const TickData& tick_data);
    
    zmq::context_t context_;
    zmq::socket_t publisher_;
    
    std::queue<TickData> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    std::thread publishing_thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> published_count_{0};
    
    static constexpr size_t MAX_QUEUE_SIZE = 100000;
};

} // namespace tickshaper