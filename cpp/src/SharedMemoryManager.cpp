#include "SharedMemoryManager.h"
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <random>

namespace tickshaper {

SharedMemoryManager::SharedMemoryManager() 
    : shm_ptr_(nullptr), shm_size_(0), shm_fd_(-1), header_(nullptr), data_buffer_(nullptr) {
    
    // Generate unique shared memory name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    shm_name_ = "/tickshaper_shm_" + std::to_string(dis(gen));
}

SharedMemoryManager::~SharedMemoryManager() {
    if (shm_ptr_ != nullptr) {
        munmap(shm_ptr_, shm_size_);
    }
    
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_unlink(shm_name_.c_str());
    }
}

bool SharedMemoryManager::Initialize(size_t size) {
    if (!CreateSharedMemory(size)) {
        return false;
    }
    
    // Initialize header
    header_ = static_cast<SharedMemoryHeader*>(shm_ptr_);
    data_buffer_ = static_cast<uint8_t*>(shm_ptr_) + sizeof(SharedMemoryHeader);
    
    // Align data buffer to cache line boundary
    size_t header_size = sizeof(SharedMemoryHeader);
    size_t aligned_header_size = (header_size + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
    data_buffer_ = static_cast<uint8_t*>(shm_ptr_) + aligned_header_size;
    
    header_->buffer_size = shm_size_ - aligned_header_size;
    header_->max_message_size = MAX_MESSAGE_SIZE;
    header_->write_index.store(0);
    header_->read_index.store(0);
    header_->initialized.store(true);
    
    std::cout << "Shared memory initialized: " << shm_size_ << " bytes, "
              << "data buffer: " << header_->buffer_size << " bytes" << std::endl;
    
    return true;
}

bool SharedMemoryManager::WriteMessage(const void* data, size_t size) {
    if (!header_ || !header_->initialized.load() || size > MAX_MESSAGE_SIZE) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // Check if there's enough space (including size header)
    size_t total_size = sizeof(uint32_t) + size;
    if (GetAvailableSpace() < total_size) {
        return false; // Buffer full
    }
    
    // Write message size first
    void* write_ptr = GetWritePointer(sizeof(uint32_t));
    if (!write_ptr) return false;
    
    *static_cast<uint32_t*>(write_ptr) = static_cast<uint32_t>(size);
    AdvanceWritePointer(sizeof(uint32_t));
    
    // Write message data
    write_ptr = GetWritePointer(size);
    if (!write_ptr) return false;
    
    memcpy(write_ptr, data, size);
    AdvanceWritePointer(size);
    
    return true;
}

bool SharedMemoryManager::ReadMessage(void* buffer, size_t& size) {
    if (!header_ || !header_->initialized.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(read_mutex_);
    
    if (IsEmpty()) {
        return false;
    }
    
    // Read message size first
    void* read_ptr = GetReadPointer();
    if (!read_ptr) return false;
    
    uint32_t message_size = *static_cast<uint32_t*>(read_ptr);
    AdvanceReadPointer(sizeof(uint32_t));
    
    if (message_size > size) {
        // Buffer too small
        size = message_size;
        return false;
    }
    
    // Read message data
    read_ptr = GetReadPointer();
    if (!read_ptr) return false;
    
    memcpy(buffer, read_ptr, message_size);
    AdvanceReadPointer(message_size);
    
    size = message_size;
    return true;
}

size_t SharedMemoryManager::GetAvailableSpace() const {
    if (!header_) return 0;
    
    uint64_t write_idx = header_->write_index.load();
    uint64_t read_idx = header_->read_index.load();
    
    if (write_idx >= read_idx) {
        return header_->buffer_size - (write_idx - read_idx);
    } else {
        return read_idx - write_idx;
    }
}

size_t SharedMemoryManager::GetUsedSpace() const {
    return header_ ? header_->buffer_size - GetAvailableSpace() : 0;
}

bool SharedMemoryManager::IsEmpty() const {
    if (!header_) return true;
    return header_->write_index.load() == header_->read_index.load();
}

bool SharedMemoryManager::CreateSharedMemory(size_t size) {
    shm_size_ = size;
    
    // Create shared memory object
    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_ == -1) {
        std::cerr << "Failed to create shared memory object" << std::endl;
        return false;
    }
    
    // Set size
    if (ftruncate(shm_fd_, shm_size_) == -1) {
        std::cerr << "Failed to set shared memory size" << std::endl;
        return false;
    }
    
    // Map memory
    shm_ptr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
    if (shm_ptr_ == MAP_FAILED) {
        std::cerr << "Failed to map shared memory" << std::endl;
        shm_ptr_ = nullptr;
        return false;
    }
    
    return true;
}

void* SharedMemoryManager::GetWritePointer(size_t size) {
    uint64_t write_idx = header_->write_index.load();
    uint64_t buffer_pos = write_idx % header_->buffer_size;
    
    // Check for wrap-around
    if (buffer_pos + size > header_->buffer_size) {
        return nullptr; // Would wrap around, need to handle this case
    }
    
    return data_buffer_ + buffer_pos;
}

void* SharedMemoryManager::GetReadPointer() {
    uint64_t read_idx = header_->read_index.load();
    uint64_t buffer_pos = read_idx % header_->buffer_size;
    
    return data_buffer_ + buffer_pos;
}

void SharedMemoryManager::AdvanceWritePointer(size_t size) {
    header_->write_index.fetch_add(size);
}

void SharedMemoryManager::AdvanceReadPointer(size_t size) {
    header_->read_index.fetch_add(size);
}

} // namespace tickshaper