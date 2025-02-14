// videoCodec.h
#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H

#include <opencv2/opencv.hpp>
#include <string>
#include "../../Common/bitStream.h"
#include "../../Common/golomb.h"


#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <numeric>
#include <array>

using namespace std;
using namespace cv;

// Forward declarations
class BitCounter;
class TestBitStream;

// Structures
struct MotionVector {
    int dx;  // x displacement
    int dy;  // y displacement
    MotionVector(int x = 0, int y = 0) : dx(x), dy(y) {}
};

struct BlockMatchingParams {
    int blockSize;      // Size of blocks for motion estimation
    int searchRange;    // Search range in pixels

    BlockMatchingParams(int bs = 16, int sr = 16)
        : blockSize(bs), searchRange(sr) {}
};

// Main encoding/decoding functions
    void encodeRawVideo(  array<unsigned long long, 8>& stats,
                            const std::string& inputFile,
                            const std::string& outputFile,
                            const BlockMatchingParams& params = BlockMatchingParams(),
                            int frame_period = -1,
                            int shiftBits = 0);

    void decodeRawVideo(const std::string& inputFile,
                            const std::string& outputFile);

// Logging function
    void logResults(const string& inputFile, const string& outputFile,
                const string& operation, double elapsedTime,
                const array<unsigned long long, 8>* stats = nullptr);
// Helper functions for frame processing
    void parseY4MHeader(std::ifstream& input, int& width, int& height, int& frame_count,
                   int& uvWidth, int& uvHeight, int& uvFrameSize, int& yFrameSize);
    int countY4MFrames(ifstream& input);
    int predictPixel(const cv::Mat& image, int x, int y);

// Frame encoding/decoding
    void encodeFrameIntra(const cv::Mat& frame,
                            Golomb& golomb,
                            BitStream& stream,
                            int shiftBits);

    void decodeFrameIntra(cv::Mat& frame,
                            Golomb& golomb,
                            BitStream& stream,
                            int shiftBits);

    void encodeFrameInter(const cv::Mat& currentFrame,
                            const cv::Mat& referenceFrame,
                            Golomb& golomb,
                            BitStream& stream,
                            unsigned long long& counter1,
                            unsigned long long& counter2,
                            int shiftBits,
                            const BlockMatchingParams& params);

    void decodeFrameInter(cv::Mat& frame,
                            const cv::Mat& referenceFrame,
                            Golomb& golomb,
                            BitStream& stream,
                            int shiftBits,
                            const BlockMatchingParams& params);

// Motion estimation helpers
    int calculateSAD(const cv::Mat& currentBlock,
                        const cv::Mat& referenceBlock);

    MotionVector findBestMotionVector(const cv::Mat& currentBlock,
                                        const cv::Mat& referenceFrame,
                                        int blockX,
                                        int blockY,
                                        const BlockMatchingParams& params);

    cv::Mat getPredictedBlock(const cv::Mat& referenceFrame,
                                int x,
                                int y,
                                int width,
                                int height,
                                const MotionVector& mv);

// Golomb parameter calculation
    int calculateOptimalM(const cv::Mat& frame);
    int calculateOptimalMInter(const cv::Mat& currentFrame,
                                const cv::Mat& referenceFrame,
                                const BlockMatchingParams& params);


#endif // VIDEO_CODEC_H