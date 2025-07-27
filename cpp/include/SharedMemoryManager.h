#pragma once

#include <string>
#include <cstddef>
#include <sys/mman.h>
#include <atomic>
#include <mutex>

namespace tickshaper {

struct SharedMemoryHeader {
    std::atomic<uint64_t> write_index{0};
    std::atomic<uint64_t> read_index{0};
    uint64_t buffer_size;
    uint64_t max_message_size;
    std::atomic<bool> initialized{false};
};

class SharedMemoryManager {
public:
    SharedMemoryManager();
    ~SharedMemoryManager();
    
    bool Initialize(size_t size);
    bool WriteMessage(const void* data, size_t size);
    bool ReadMessage(void* buffer, size_t& size);
    
    size_t GetAvailableSpace() const;
    size_t GetUsedSpace() const;
    bool IsEmpty() const;
    
private:
    bool CreateSharedMemory(size_t size);
    void* GetWritePointer(size_t size);
    void* GetReadPointer();
    void AdvanceWritePointer(size_t size);
    void AdvanceReadPointer(size_t size);
    
    void* shm_ptr_;
    size_t shm_size_;
    int shm_fd_;
    std::string shm_name_;
    
    SharedMemoryHeader* header_;
    uint8_t* data_buffer_;
    
    mutable std::mutex write_mutex_;
    mutable std::mutex read_mutex_;
    
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;
};

} // namespace tickshaper