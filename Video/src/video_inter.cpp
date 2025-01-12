#include <iostream>
#include <fstream>
#include <vector>
#include "../../Common/bitStream.h"
#include "../../Common/golomb.h"
#include <opencv2/opencv.hpp>
#include <regex>


#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;
using namespace cv;

// Function to read width and height from Y4M header
void parseY4MHeader(ifstream& input, int& width, int& height) {
    string line;
    getline(input, line);

    regex re(R"(YUV4MPEG2\s+W(\d+)\s+H(\d+))");
    smatch match;

    if (regex_search(line, match, re)) {
        width = stoi(match[1].str());
        height = stoi(match[2].str());
    } else {
        cerr << "Error: Invalid Y4M header format." << endl;
        exit(1);
    }
}


// Prediction function for JPEG-LS
int predictPixel(const Mat& image, int x, int y) {
    int A = (x > 0) ? image.at<uchar>(y, x - 1) : 0;
    int B = (y > 0) ? image.at<uchar>(y - 1, x) : 0;
    int C = (x > 0 && y > 0) ? image.at<uchar>(y - 1, x - 1) : 0;

    if (C >= max(A, B)) {
        return min(A, B);
    } else if (C <= min(A, B)) {
        return max(A, B);
    } else {
        return A + B - C;
    }
}

// Encoding a single frame (Y channel only)
void encodeFrameIntra(const Mat& frame, Golomb& golomb, BitStream& stream) {
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            int predicted = predictPixel(frame, x, y);
            int residual = frame.at<uchar>(y, x) - predicted;
            golomb.encode(stream, residual);
        }
    }
}

// Decoding a single frame (Y channel only)
void decodeFrameIntra(Mat& frame, Golomb& golomb, BitStream& stream) {
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            try {
                int residual = golomb.decode(stream);
                int predicted = predictPixel(frame, x, y);
                frame.at<uchar>(y, x) = saturate_cast<uchar>(residual + predicted);
            } catch (const std::runtime_error& e) {
                cerr << "Error during decoding: " << e.what() << endl;
                return;  // Exit the decoding loop if EOF or any error occurs
            }
        }
    }
}


// Structure to hold motion vector information
struct MotionVector {
    int dx;  // x displacement
    int dy;  // y displacement
    MotionVector(int x = 0, int y = 0) : dx(x), dy(y) {}
};

// Structure to hold block matching parameters
struct BlockMatchingParams {
    int blockSize;      // Size of blocks for motion estimation
    int searchRange;    // Search range in pixels (e.g., 16 for ±16 pixels)

    BlockMatchingParams(int bs = 16, int sr = 16)
        : blockSize(bs), searchRange(sr) {}
};

// Calculate Sum of Absolute Differences (SAD) between two blocks
int calculateSAD(const Mat& currentBlock, const Mat& referenceBlock) {
    CV_Assert(currentBlock.size() == referenceBlock.size());
    int sad = 0;
    for (int y = 0; y < currentBlock.rows; ++y) {
        for (int x = 0; x < currentBlock.cols; ++x) {
            sad += abs(currentBlock.at<uchar>(y, x) - referenceBlock.at<uchar>(y, x));
        }
    }
    return sad;
}




// Find the best motion vector using full search within search range
MotionVector findBestMotionVector(const Mat& currentBlock, const Mat& referenceFrame,
                                int blockX, int blockY, const BlockMatchingParams& params) {
    MotionVector bestMV(0, 0);
    int minSAD = INT_MAX;

    // Get current block dimensions (might be smaller at frame borders)
    int blockHeight = currentBlock.rows;
    int blockWidth = currentBlock.cols;

    // Search area boundaries in reference frame
    int startX = max(-params.searchRange, -blockX);
    int endX = min(params.searchRange, referenceFrame.cols - blockX - blockWidth);
    int startY = max(-params.searchRange, -blockY);
    int endY = min(params.searchRange, referenceFrame.rows - blockY - blockHeight);

    // Full search within the search range
    for (int dy = startY; dy <= endY; ++dy) {
        for (int dx = startX; dx <= endX; ++dx) {
            // Get candidate block from reference frame
            int refX = blockX + dx;
            int refY = blockY + dy;

            Mat candidateBlock = referenceFrame(
                Range(refY, refY + blockHeight),
                Range(refX, refX + blockWidth)
            );

            // Calculate SAD for this candidate
            int sad = calculateSAD(currentBlock, candidateBlock);

            // Update best motion vector if this SAD is lower
            if (sad < minSAD) {
                minSAD = sad;
                bestMV = MotionVector(dx, dy);
            }
        }
    }

    return bestMV;
}

// Helper function to get predicted block using motion vector
Mat getPredictedBlock(const Mat& referenceFrame, int x, int y,
                     int width, int height, const MotionVector& mv) {
    // Calculate reference block coordinates with motion vector
    int refX = x + mv.dx;
    int refY = y + mv.dy;

    // Clamp coordinates to frame boundaries
    refX = max(0, min(refX, referenceFrame.cols - width));
    refY = max(0, min(refY, referenceFrame.rows - height));

    // Extract the predicted block from reference frame
    return referenceFrame(
        Range(refY, refY + height),
        Range(refX, refX + width)
    ).clone();
}

// Calculate optimal m for a frame based on residual distribution
int calculateOptimalM(const Mat& frame) {
    vector<int> residuals;
    residuals.reserve(frame.rows * frame.cols);

    // Calculate residuals for the frame
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            int predicted = predictPixel(frame, x, y);
            int residual = frame.at<uchar>(y, x) - predicted;
            residuals.push_back(residual);
        }
    }

    // Calculate mean absolute value of residuals
    double sum = 0;
    for (int residual : residuals) {
        sum += abs(residual);
    }
    double average = sum / residuals.size();

    // Calculate optimal m using the provided formula
    int m = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (average + 1)))));

    // Ensure m is at least 1 and not too large
    return max(2, min(m, 16));
}

// Calculate optimal m for inter-frame residuals
int calculateOptimalMInter(const Mat& currentFrame, const Mat& referenceFrame,
                         const BlockMatchingParams& params) {
    vector<int> residuals;
    residuals.reserve(currentFrame.rows * currentFrame.cols);

    // Process each block to collect residuals
    for (int y = 0; y < currentFrame.rows; y += params.blockSize) {
        for (int x = 0; x < currentFrame.cols; x += params.blockSize) {
            int currentBlockHeight = min(params.blockSize, currentFrame.rows - y);
            int currentBlockWidth = min(params.blockSize, currentFrame.cols - x);

            Mat currentBlock = currentFrame(
                Range(y, y + currentBlockHeight),
                Range(x, x + currentBlockWidth)
            );

            MotionVector mv = findBestMotionVector(currentBlock, referenceFrame,
                                                 x, y, params);

            Mat predictedBlock = getPredictedBlock(referenceFrame, x, y,
                                                 currentBlockWidth, currentBlockHeight,
                                                 mv);

            // Collect residuals
            for (int by = 0; by < currentBlockHeight; ++by) {
                for (int bx = 0; bx < currentBlockWidth; ++bx) {
                    int residual = currentBlock.at<uchar>(by, bx) -
                                 predictedBlock.at<uchar>(by, bx);
                    residuals.push_back(residual);
                }
            }
        }
    }

    // Calculate mean absolute value of residuals
    double sum = 0;
    for (int residual : residuals) {
        sum += abs(residual);
    }
    double average = sum / residuals.size();

    // Calculate optimal m using the provided formula
    int m = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (average + 1)))));
    return max(2, min(m, 16));
}

// Encode inter-predicted frame with customizable parameters
void encodeFrameInter(const Mat& currentFrame, const Mat& referenceFrame,
                      Golomb& golomb, BitStream& stream,
                      const BlockMatchingParams& params = BlockMatchingParams()) {
    const int rows = currentFrame.rows;
    const int cols = currentFrame.cols;

    // Process each block
    for (int y = 0; y < rows; y += params.blockSize) {
        for (int x = 0; x < cols; x += params.blockSize) {
            // Actual block size might be smaller at borders
            int currentBlockHeight = min(params.blockSize, rows - y);
            int currentBlockWidth = min(params.blockSize, cols - x);

            // Get the current block
            Mat currentBlock = currentFrame(
                Range(y, y + currentBlockHeight),
                Range(x, x + currentBlockWidth)
            );

            // Find best motion vector for this block
            MotionVector mv = findBestMotionVector(currentBlock, referenceFrame,
                                                 x, y, params);

            // Encode motion vector (using differential coding)
            golomb.encode(stream, mv.dx);
            golomb.encode(stream, mv.dy);

            // Get predicted block from reference frame using motion vector
            Mat predictedBlock = getPredictedBlock(referenceFrame, x, y,
                                                 currentBlockWidth, currentBlockHeight,
                                                 mv);

            // Encode residuals for the block
            for (int by = 0; by < currentBlockHeight; ++by) {
                for (int bx = 0; bx < currentBlockWidth; ++bx) {
                    int residual = currentBlock.at<uchar>(by, bx) -
                                 predictedBlock.at<uchar>(by, bx);
                    golomb.encode(stream, residual);
                }
            }
        }
    }
}

// Decode inter-predicted frame with customizable parameters
void decodeFrameInter(Mat& frame, const Mat& referenceFrame,
                      Golomb& golomb, BitStream& stream,
                      const BlockMatchingParams& params = BlockMatchingParams()) {
    const int rows = frame.rows;
    const int cols = frame.cols;

    // Process each block
    for (int y = 0; y < rows; y += params.blockSize) {
        for (int x = 0; x < cols; x += params.blockSize) {
            int currentBlockHeight = min(params.blockSize, rows - y);
            int currentBlockWidth = min(params.blockSize, cols - x);

            // Decode motion vector
            int dx = golomb.decode(stream);
            int dy = golomb.decode(stream);
            MotionVector mv(dx, dy);

            // Get predicted block from reference frame
            Mat predictedBlock = getPredictedBlock(referenceFrame, x, y,
                                                 currentBlockWidth, currentBlockHeight,
                                                 mv);

            // Decode residuals and reconstruct the block
            for (int by = 0; by < currentBlockHeight; ++by) {
                for (int bx = 0; bx < currentBlockWidth; ++bx) {
                    int residual = golomb.decode(stream);
                    frame.at<uchar>(y + by, x + bx) = saturate_cast<uchar>(
                        predictedBlock.at<uchar>(by, bx) + residual
                    );
                }
            }
        }
    }
}

// Modify encodeRawVideo function to use automatic m selection
void encodeRawVideo(const string& inputFile, const string& outputFile,
                    const BlockMatchingParams& params = BlockMatchingParams()) {
    ifstream input(inputFile, ios::binary);
    if (!input) {
        cerr << "Error: Could not open input file." << endl;
        return;
    }

    int width, height;
    parseY4MHeader(input, width, height);
    BitStream stream(outputFile, true);

    // Write header information
    stream.writeBits(width, 16);
    stream.writeBits(height, 16);
    stream.writeBits(params.blockSize, 8);
    stream.writeBits(params.searchRange, 8);

    int frameSize = width * height;
    Mat currentFrame(height, width, CV_8UC1);
    Mat referenceFrame(height, width, CV_8UC1);
    int frameCount = 0;

    while (input.read(reinterpret_cast<char*>(currentFrame.data), frameSize)) {
        // Determine frame type
        bool isIntra = (frameCount % 8 == 0);  // Every 8th frame is intra
        stream.writeBits(isIntra ? 0 : 1, 1);

        // Calculate optimal m for this frame
        int optimalM;
        if (isIntra) {
            optimalM = calculateOptimalM(currentFrame);
        } else {
            optimalM = calculateOptimalMInter(currentFrame, referenceFrame, params);
        }

        // Write m value for this frame
        stream.writeBits(optimalM, 8);
        Golomb golomb(optimalM, true);

        if (isIntra) {
            encodeFrameIntra(currentFrame, golomb, stream);
            currentFrame.copyTo(referenceFrame);
        } else {
            encodeFrameInter(currentFrame, referenceFrame, golomb, stream, params);
        }

        frameCount++;
    }

    cout << "Video encoded successfully." << endl;
}

// Modify decodeRawVideo to read per-frame m values
void decodeRawVideo(const string& inputFile, const string& outputFile) {
    BitStream stream(inputFile, false);
    int width = stream.readBits(16);
    int height = stream.readBits(16);
    int blockSize = stream.readBits(8);
    int searchRange = stream.readBits(8);

    BlockMatchingParams params(blockSize, searchRange);

    int frameSize = width * height;
    ofstream output(outputFile, ios::binary);
    output << "YUV4MPEG2 W" << width << " H" << height << " F30:1 Ip A0:0\n";

    Mat currentFrame(height, width, CV_8UC1, Scalar(0));
    Mat referenceFrame(height, width, CV_8UC1, Scalar(0));

    while (!stream.eof()) {
        try {
            // Read frame type
            bool isIntra = (stream.readBits(1) == 0);

            // Read m value for this frame
            int m = stream.readBits(8);
            Golomb golomb(m, true);

            if (isIntra) {
                decodeFrameIntra(currentFrame, golomb, stream);
                currentFrame.copyTo(referenceFrame);
            } else {
                decodeFrameInter(currentFrame, referenceFrame, golomb, stream, params);
            }

            output.write(reinterpret_cast<const char*>(currentFrame.data), frameSize);

        } catch (const std::runtime_error& e) {
            cerr << "Decoding finished or encountered error: " << e.what() << endl;
            break;
        }
    }
}

void logResults(const string& inputFile, const string& outputFile,
                const string& operation, double elapsedTime) {
    ifstream in(inputFile, ios::binary | ios::ate);
    ifstream out(outputFile, ios::binary | ios::ate);

    if (!in || !out) {
        cerr << "Error: Could not open input or output file to log results." << endl;
        return;
    }

    auto inputSize = in.tellg();
    auto outputSize = out.tellg();

    if (inputSize == 0) {
        cerr << "Error: Input file size is zero. Cannot compute size ratio." << endl;
        return;
    }

    double sizeRatio = static_cast<double>(outputSize) / static_cast<double>(inputSize);

    // Prepare and print the log message
    cout << "Operation: " << operation << "\n"
         << "Input File: " << inputFile << "\n"
         << "Output File: " << outputFile << "\n"
         << "Input Size: " << inputSize << " bytes\n"
         << "Output Size: " << outputSize << " bytes\n"
         << fixed << setprecision(3)
         << "Size Ratio (Output/Input): " << sizeRatio << "\n"
         << "Time Taken: " << elapsedTime << " seconds\n"
         << "---------------------------------------------\n";
}


// Update main function to remove m parameter
int main(int argc, char** argv) {
    if (argc < 4) {
        cout << "Usage:\n";
        cout << "./video_frame -encode <input_raw_video> <output_encoded_file>\n";
        cout << "./video_frame -decode <input_encoded_file> <output_raw_video>\n";
        return 1;
    }

    string action = argv[1];
    auto startTime = chrono::high_resolution_clock::now();

    if (action == "-encode" && argc == 4) {
        string inputFile = argv[2];
        string outputFile = argv[3];

        encodeRawVideo(inputFile, outputFile);

        auto endTime = chrono::high_resolution_clock::now();
        double elapsedTime = chrono::duration<double>(endTime - startTime).count();
        logResults(inputFile, outputFile, "EncodingFrames", elapsedTime);

    } else if (action == "-decode" && argc == 4) {
        string inputFile = argv[2];
        string outputFile = argv[3];

        decodeRawVideo(inputFile, outputFile);

        auto endTime = chrono::high_resolution_clock::now();
        double elapsedTime = chrono::duration<double>(endTime - startTime).count();
        logResults(inputFile, outputFile, "DecodingFrames", elapsedTime);

    } else {
        cout << "Invalid arguments. Please use the following format:\n";
        cout << "./video_frame -encode <input_raw_video> <output_encoded_file>\n";
        cout << "./video_frame -decode <input_encoded_file> <output_raw_video>\n";
    }

    return 0;
}