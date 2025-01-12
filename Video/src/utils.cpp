#include "VideoCodec.h"


using namespace std;
using namespace cv;

// Function to read width and height from Y4M header
void parseY4MHeader(ifstream& input, int& width, int& height, int& frame_count) {
    input.clear();  // Clear any EOF flags
    input.seekg(0, ios::beg);  // Move the file pointer back to the beginning
    frame_count = countY4MFrames(input);
    input.clear();  // Clear any EOF flags
    input.seekg(0, ios::beg);  // Move the file pointer back to the beginning

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

// Function to count frames in a Y4M file
int countY4MFrames(ifstream& input) {
    string line;
    int frameCount = 0;

    // Skip over the header part of the file
    while (getline(input, line)) {
        if (line.find("YUV4MPEG2") != string::npos) {
            // We've found the header, now start reading frames
            break;
        }
    }

    // Now, read through the file and count frames
    while (getline(input, line)) {
        if (line.find("FRAME") != string::npos) {
            frameCount++;
        }
    }

    return frameCount;
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