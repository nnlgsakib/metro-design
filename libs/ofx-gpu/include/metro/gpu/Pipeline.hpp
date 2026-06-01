#pragma once

#include <cstddef>
#include <cstdint>

namespace metro {
namespace gpu {

enum class DeviceType {
    None,
    CUDA
};

struct DeviceInfo {
    DeviceType type = DeviceType::None;
    int deviceCount = 0;
    char deviceName[256] = {};
    size_t totalMemory = 0;
    int computeCapabilityMajor = 0;
    int computeCapabilityMinor = 0;
};

DeviceInfo probeDevices();

class Buffer {
public:
    Buffer();
    ~Buffer();

    bool allocate(size_t bytes);
    void release();
    bool upload(const void *hostData, size_t bytes);
    bool download(void *hostData, size_t bytes) const;

    bool isValid() const { return devicePtr_ != nullptr; }
    size_t size() const { return size_; }
    void *devicePtr() const { return devicePtr_; }

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&other) noexcept;
    Buffer &operator=(Buffer &&other) noexcept;

private:
    void *devicePtr_ = nullptr;
    size_t size_ = 0;
};

class Stream {
public:
    Stream();
    ~Stream();
    bool create();
    void destroy();
    bool isValid() const { return stream_ != nullptr; }
    bool synchronize();
    void *nativeHandle() const { return stream_; }

    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    Stream(Stream &&other) noexcept;
    Stream &operator=(Stream &&other) noexcept;

private:
    void *stream_ = nullptr;
};

struct KernelLaunchParams {
    const void *func = nullptr;
    int gridX = 1, gridY = 1, gridZ = 1;
    int blockX = 256, blockY = 1, blockZ = 1;
    int sharedMemBytes = 0;
    Stream *stream = nullptr;
};

bool launchKernel(const KernelLaunchParams &params, void **args);

enum class MemoryKind {
    HostPinned,
    Device
};

bool allocateMemory(void **ptr, size_t bytes, MemoryKind kind);
bool freeMemory(void *ptr, MemoryKind kind);

} // namespace gpu
} // namespace metro
