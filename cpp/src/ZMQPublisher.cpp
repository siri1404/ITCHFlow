#include "ZMQPublisher.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace tickshaper {

ZMQPublisher::ZMQPublisher() : context_(1), publisher_(context_, ZMQ_PUB) {
}

ZMQPublisher::~ZMQPublisher() {
    Stop();
}

bool ZMQPublisher::Initialize(const std::string& endpoint) {
    try {
        // Set high water mark to prevent blocking
        int hwm = 10000;
        publisher_.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
        
        // Bind to endpoint
        publisher_.bind(endpoint);
        
        running_.store(true);
        publishing_thread_ = std::thread([this]() { PublishingLoop(); });
        
        std::cout << "ZMQ Publisher initialized on " << endpoint << std::endl;
        return true;
        
    } catch (const zmq::error_t& e) {
        std::cerr << "ZMQ Publisher initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void ZMQPublisher::Publish(const TickData& tick_data) {
    if (!running_.load()) {
        return;
    }
    
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // Drop messages if queue is full (backpressure handling)
    if (message_queue_.size() >= MAX_QUEUE_SIZE) {
        message_queue_.pop();
    }
    
    message_queue_.push(tick_data);
    lock.unlock();
    
    queue_cv_.notify_one();
}

void ZMQPublisher::Stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    queue_cv_.notify_all();
    
    if (publishing_thread_.joinable()) {
        publishing_thread_.join();
    }
    
    publisher_.close();
    std::cout << "ZMQ Publisher stopped. Published " << published_count_.load() << " messages" << std::endl;
}

void ZMQPublisher::PublishingLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait(lock, [this]() {
            return !message_queue_.empty() || !running_.load();
        });
        
        if (!running_.load()) {
            break;
        }
        
        // Process batch of messages
        std::vector<TickData> batch;
        batch.reserve(std::min(message_queue_.size(), size_t(1000)));
        
        while (!message_queue_.empty() && batch.size() < 1000) {
            batch.push_back(message_queue_.front());
            message_queue_.pop();
        }
        
        lock.unlock();
        
        // Publish batch
        for (const auto& tick_data : batch) {
            try {
                std::string serialized = SerializeTickData(tick_data);
                
                zmq::message_t message(serialized.size());
                memcpy(message.data(), serialized.c_str(), serialized.size());
                
                publisher_.send(message, zmq::send_flags::dontwait);
                published_count_.fetch_add(1);
                
            } catch (const zmq::error_t& e) {
                if (e.num() != EAGAIN) {
                    std::cerr << "ZMQ send error: " << e.what() << std::endl;
                }
            }
        }
    }
}

std::string ZMQPublisher::SerializeTickData(const TickData& tick_data) {
    // Simple JSON serialization
    std::ostringstream oss;
    oss << "{"
        << "\"timestamp\":" << tick_data.timestamp << ","
        << "\"symbol_id\":" << tick_data.symbol_id << ","
        << "\"price\":" << tick_data.price << ","
        << "\"size\":" << tick_data.size << ","
        << "\"side\":\"" << tick_data.side << "\","
        << "\"message_type\":\"" << static_cast<char>(tick_data.message_type) << "\""
        << "}";
    
    return oss.str();
}

} // namespace tickshaper