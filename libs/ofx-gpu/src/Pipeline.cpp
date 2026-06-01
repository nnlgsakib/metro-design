#include "metro/gpu/Pipeline.hpp"
#include <cstring>
#include <cstdlib>

namespace metro {
namespace gpu {

DeviceInfo probeDevices()
{
    DeviceInfo info;
#if METRO_HAVE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err == cudaSuccess && count > 0) {
        info.type = DeviceType::CUDA;
        info.deviceCount = count;
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
            std::strncpy(info.deviceName, prop.name, sizeof(info.deviceName) - 1);
            info.totalMemory = prop.totalGlobalMem;
            info.computeCapabilityMajor = prop.major;
            info.computeCapabilityMinor = prop.minor;
        }
    }
#else
    (void)info;
#endif
    return info;
}

Buffer::Buffer() = default;
Buffer::~Buffer() { release(); }

bool Buffer::allocate(size_t bytes)
{
    release();
#if METRO_HAVE_CUDA
    cudaError_t err = cudaMalloc(&devicePtr_, bytes);
    if (err == cudaSuccess) {
        size_ = bytes;
        return true;
    }
    return false;
#else
    (void)bytes;
    return false;
#endif
}

void Buffer::release()
{
#if METRO_HAVE_CUDA
    if (devicePtr_) {
        cudaFree(devicePtr_);
        devicePtr_ = nullptr;
        size_ = 0;
    }
#endif
}

bool Buffer::upload(const void *hostData, size_t bytes)
{
    if (!devicePtr_ || bytes > size_) return false;
#if METRO_HAVE_CUDA
    return cudaMemcpy(devicePtr_, hostData, bytes, cudaMemcpyHostToDevice) == cudaSuccess;
#else
    (void)hostData;
    (void)bytes;
    return false;
#endif
}

bool Buffer::download(void *hostData, size_t bytes) const
{
    if (!devicePtr_ || bytes > size_) return false;
#if METRO_HAVE_CUDA
    return cudaMemcpy(hostData, devicePtr_, bytes, cudaMemcpyDeviceToHost) == cudaSuccess;
#else
    (void)hostData;
    (void)bytes;
    return false;
#endif
}

Buffer::Buffer(Buffer &&other) noexcept
    : devicePtr_(other.devicePtr_), size_(other.size_)
{
    other.devicePtr_ = nullptr;
    other.size_ = 0;
}

Buffer &Buffer::operator=(Buffer &&other) noexcept
{
    if (this != &other) {
        release();
        devicePtr_ = other.devicePtr_;
        size_ = other.size_;
        other.devicePtr_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

Stream::Stream() = default;
Stream::~Stream() { destroy(); }

bool Stream::create()
{
    destroy();
#if METRO_HAVE_CUDA
    return cudaStreamCreate(reinterpret_cast<cudaStream_t *>(&stream_)) == cudaSuccess;
#else
    return false;
#endif
}

void Stream::destroy()
{
#if METRO_HAVE_CUDA
    if (stream_) {
        cudaStreamDestroy(reinterpret_cast<cudaStream_t>(stream_));
        stream_ = nullptr;
    }
#endif
}

bool Stream::synchronize()
{
#if METRO_HAVE_CUDA
    if (stream_) {
        return cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(stream_)) == cudaSuccess;
    }
#endif
    return false;
}

Stream::Stream(Stream &&other) noexcept
    : stream_(other.stream_)
{
    other.stream_ = nullptr;
}

Stream &Stream::operator=(Stream &&other) noexcept
{
    if (this != &other) {
        destroy();
        stream_ = other.stream_;
        other.stream_ = nullptr;
    }
    return *this;
}

bool launchKernel(const KernelLaunchParams &params, void **args)
{
#if METRO_HAVE_CUDA
    cudaStream_t s = params.stream ? reinterpret_cast<cudaStream_t>(params.stream->nativeHandle()) : nullptr;
    cudaError_t err = cudaLaunchKernel(
        const_cast<void *>(params.func),
        dim3(params.gridX, params.gridY, params.gridZ),
        dim3(params.blockX, params.blockY, params.blockZ),
        args,
        params.sharedMemBytes,
        s
    );
    return err == cudaSuccess;
#else
    (void)params;
    (void)args;
    return false;
#endif
}

bool allocateMemory(void **ptr, size_t bytes, MemoryKind kind)
{
#if METRO_HAVE_CUDA
    if (kind == MemoryKind::HostPinned) {
        return cudaHostAlloc(ptr, bytes, cudaHostAllocDefault) == cudaSuccess;
    }
#endif
    (void)kind;
    *ptr = std::malloc(bytes);
    return *ptr != nullptr;
}

bool freeMemory(void *ptr, MemoryKind kind)
{
#if METRO_HAVE_CUDA
    if (kind == MemoryKind::HostPinned) {
        return cudaFreeHost(ptr) == cudaSuccess;
    }
#endif
    (void)kind;
    std::free(ptr);
    return true;
}

} // namespace gpu
} // namespace metro
