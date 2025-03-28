#ifndef FRAME_POOL_H
#define FRAME_POOL_H

#include <cstdint>
#include <memory>
#include <vector>
#include <cassert>

#include <libcamera/base/span.h>
#include <libcamera/framebuffer.h>
#include "image.h"

class PooledFrame {
public:
    friend class FramePool;

    PooledFrame() = default;
    unsigned int numPlanes() const { return planeData_.size(); }
    libcamera::Span<const uint8_t> data(unsigned int plane) const {
        assert(plane < planeData_.size());
        return planeData_[plane];
    }
    uint64_t sequenceNumber() const { return sequenceNumber_; }

private:
    std::vector<libcamera::Span<uint8_t>> planeData_;
    uint64_t sequenceNumber_ = 0;
};

// Memory pool for frame data with built-in ring buffer functionality
class FramePool {
public:
    // Create a pool based on the structure of a sample frame
    static std::unique_ptr<FramePool> create(const Image& sampleFrame, size_t frameCount);
    static std::unique_ptr<FramePool> create(const Image& sampleFrame, uint8_t seconds, float frameRate);
    ~FramePool() = default;

    // Copy data from a libcamera Image to the next available frame slot
    // Returns a pointer to the stored frame
    const PooledFrame* storeFrame(const Image& image);
    const PooledFrame* getOldestFrame() const;
    const PooledFrame* getLatestFrame() const;
    const PooledFrame* getFrame(size_t index) const;

    bool isFull() const { return size() == capacity(); }
    size_t capacity() const { return frames_.size(); }
    size_t size() const { return std::min(frameCount_, capacity()); }
    size_t totalFramesStored() const { return frameCount_; }

private:
    FramePool() = default;
    std::vector<std::vector<uint8_t>> poolMemory_; // Pre-allocated memory for all planes of all frames
    std::vector<PooledFrame> frames_; // Array of frame objects that point into the pool memory
    size_t currentPos_ = 0;           // Current position in the ring buffer (where next frame will be written)
    size_t frameCount_ = 0;           // Total number of frames stored (can exceed capacity)
};

size_t getFreeRam();

#endif // FRAME_POOL_H
