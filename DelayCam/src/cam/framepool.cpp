#include "framepool.h"
#include "util/logger.h"
#include <algorithm>
#include <fstream>
#include <cstring>

std::unique_ptr<FramePool> FramePool::create(const Image& sampleFrame, size_t frameCount)
{
    // Calculate the required ram size
    size_t totalSize = 0;
    const unsigned int numPlanes = sampleFrame.numPlanes();
    for (unsigned int plane = 0; plane < numPlanes; plane++)
        totalSize += sampleFrame.data(plane).size() * frameCount;

    // Check if there is enough free ram
    size_t freeSize = getFreeRam();
    if (totalSize >= freeSize) {
        dcError(QString("Required RAM: %1MB, Free RAM: %2MB").arg(totalSize / 1048576).arg(freeSize / 1048576));
        return nullptr;
    } else dcInfo(QString("Required RAM: %1MB, Free RAM: %2MB").arg(totalSize / 1048576).arg(freeSize / 1048576));

    // Create pool
    std::unique_ptr<FramePool> pool(new FramePool());

    // Reserve space for all frames
    pool->frames_.resize(frameCount);
    pool->poolMemory_.resize(numPlanes);

    // Pre-allocate memory for each plane across all frames
    for (unsigned int plane = 0; plane < numPlanes; plane++) {
        const size_t planeSize = sampleFrame.data(plane).size();
        pool->poolMemory_[plane].resize(planeSize * frameCount);

        // Setup each frame's view into this plane's memory
        for (size_t frameIdx = 0; frameIdx < frameCount; frameIdx++) {
            uint8_t* planeStart = pool->poolMemory_[plane].data() + (frameIdx * planeSize);

            // Ensure the vector exists
            if (pool->frames_[frameIdx].planeData_.size() <= plane)
                pool->frames_[frameIdx].planeData_.resize(numPlanes);

            // Set up the span to point to this frame's section of the plane memory
            pool->frames_[frameIdx].planeData_[plane] =
                libcamera::Span<uint8_t>(planeStart, planeSize);
        }
    }

    // Log framepool capacity
    dcInfo(QString("Created a frame pool for %1 frames (%2MB)").arg(frameCount).arg(totalSize / 1048576));
    return pool;
}

std::unique_ptr<FramePool> FramePool::create(const Image &sampleFrame, uint8_t seconds, float frameRate)
{
    return FramePool::create(sampleFrame, (size_t)(seconds * frameRate));
}

const PooledFrame* FramePool::storeFrame(const Image& image)
{
    if (frames_.empty())
        return nullptr;

    // Get the next frame slot
    PooledFrame& frame = frames_[currentPos_];

    // Set sequence number
    frame.sequenceNumber_ = frameCount_;

    // Copy data from image to our pre-allocated memory
    const unsigned int numPlanes = std::min(image.numPlanes(), frame.numPlanes());
    for (unsigned int plane = 0; plane < numPlanes; plane++) {
        libcamera::Span<const uint8_t> srcData = image.data(plane);
        libcamera::Span<uint8_t> dstData = frame.planeData_[plane];

        // Ensure sizes match
        size_t copySize = std::min(srcData.size(), dstData.size());
        std::memcpy(dstData.data(), srcData.data(), copySize);
    }

    // Update counters
    frameCount_++;
    currentPos_ = (currentPos_ + 1) % frames_.size();
    return &frame;
}

const PooledFrame* FramePool::getOldestFrame() const
{
    if (size() == 0)
        return nullptr;

    // If not full yet, the oldest frame is the first one
    if (frameCount_ <= frames_.size()) {
        return &frames_[0];
    }

    // Otherwise, the oldest frame is at the current write position
    // (since that's the one that will be overwritten next)
    return &frames_[currentPos_];
}

const PooledFrame* FramePool::getLatestFrame() const
{
    if (size() == 0)
        return nullptr;

    // The latest frame is always one position before the current write position
    size_t latestPos = (currentPos_ == 0) ? frames_.size() - 1 : currentPos_ - 1;
    return &frames_[latestPos];
}

const PooledFrame* FramePool::getFrame(size_t index) const
{
    if (index >= size())
        return nullptr;

    // Calculate the actual position in the ring buffer
    size_t actualPos;

    // Haven't wrapped around yet, so frames are in order from 0
    if (frameCount_ <= frames_.size())
        actualPos = index;

    // Have wrapped around, oldest frame is at currentPos
    else actualPos = (currentPos_ + index) % frames_.size();
    return &frames_[actualPos];
}

size_t getFreeRam()
{
    // Get free ram size using meminfo
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    size_t freeRam = 0;

    // Decode line
    if (meminfo.is_open()) {
        while (std::getline(meminfo, line)) {
            if (line.find("MemAvailable:") != std::string::npos) {
                std::istringstream iss(line);
                std::string label;
                iss >> label >> freeRam;
                break;
            }
        }
        meminfo.close();
    }

    return freeRam * 1024;
}
