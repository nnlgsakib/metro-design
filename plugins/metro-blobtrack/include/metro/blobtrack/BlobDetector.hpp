#ifndef METRO_BLOBTRACK_BLOBDETECTOR_HPP
#define METRO_BLOBTRACK_BLOBDETECTOR_HPP

#include <cstdint>
#include <vector>

namespace metro {
namespace blobtrack {

struct BlobParams {
    float threshold{0.5f};
    int minBlobArea{10};
    int maxBlobArea{5000};
    float proximityMerge{10.0f};
    bool enabled{true};
};

struct Blob {
    int id{0};
    int label{0};
    int minX{0}, minY{0}, maxX{0}, maxY{0};
    float centroidX{0.0f}, centroidY{0.0f};
    int area{0};
    float meanIntensity{0.0f};
};

struct TrackedBlob {
    static const int kMaxTrail = 32;

    int id{0};
    int minX{0}, minY{0}, maxX{0}, maxY{0};
    float centroidX{0.0f}, centroidY{0.0f};
    int area{0};

    float trailX[kMaxTrail]{};
    float trailY[kMaxTrail]{};
    int trailHead{0};
    int trailLen{0};
};

class BlobDetector {
public:
    BlobDetector();
    ~BlobDetector();

    std::vector<Blob> detect(const uint8_t *gray, int width, int height,
                             int stride, const BlobParams &params);

    std::vector<TrackedBlob> track(const std::vector<Blob> &blobs);

    void resetTracking();

    static int nextTrackId() { static int s_id = 1; return s_id++; }

private:
    std::vector<TrackedBlob> prevBlobs_;
    int nextId_{0};
};

} // namespace blobtrack
} // namespace metro

#endif
