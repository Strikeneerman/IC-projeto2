#include "VideoCodec.h"


using namespace std;
using namespace cv;

void parseY4MHeader(ifstream& input, int& width, int& height, int& frame_count,
                   int& uvWidth, int& uvHeight, int& uvFrameSize, int& yFrameSize) {
    input.clear();
    input.seekg(0, ios::beg);

    frame_count = countY4MFrames(input);
    input.clear();
    input.seekg(0, ios::beg);

    string line;
    getline(input, line);

    // Updated regex to handle fields properly
    regex re(R"(YUV4MPEG2\s+W(\d+)\s+H(\d+)(?:\s+\S+)*\s+C([^\s]+))");
    smatch match;

    if (regex_search(line, match, re)) {
        width = stoi(match[1].str());
        height = stoi(match[2].str());
        string cParam = match[3].str();
        cout<<"cParam: "<<cParam<<endl;
        if (cParam == "420" || cParam == "420jpeg" || cParam == "420mpeg2" || cParam == "420paldv") {
            uvWidth = width / 2;
            uvHeight = height / 2;
        } else if (cParam == "422") {
            uvWidth = width / 2;
            uvHeight = height;
        } else if (cParam == "444") {
            uvWidth = width;
            uvHeight = height;
        } else if (cParam == "440") {
            uvWidth = width;
            uvHeight = height / 2;
        } else if (cParam == "411") {
            uvWidth = width / 4;
            uvHeight = height;
        } else if (cParam == "mono") {
            uvWidth = 0;
            uvHeight = 0;
        } else {
            cerr << "Warning: Unknown chroma sampling format '" << cParam << "', defaulting to 4:2:0" << endl;
            uvWidth = width / 2;
            uvHeight = height / 2;
        }

        uvFrameSize = uvWidth * uvHeight;
        yFrameSize = width * height;

    } else {
        regex re(R"(YUV4MPEG2\s+W(\d+)\s+H(\d+)(?:\s+C([^\s]+))?)");
        if (regex_search(line, match, re)) {
            width = stoi(match[1].str());
            height = stoi(match[2].str());
            uvWidth = width / 2;
            uvHeight = height / 2;
            uvFrameSize = uvWidth * uvHeight;
            yFrameSize = width * height;
        } else {
            cerr << "Error: Invalid Y4M header format." << endl;
            exit(1);
        }
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
                const string& operation, double elapsedTime,
                const array<unsigned long long, 8>* stats) {
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
    cout << "---------------------------------------------\n";
    cout << "Operation: " << operation << "\n"
         << "Input File: " << inputFile << "\n"
         << "Output File: " << outputFile << "\n"
         << "Input Size: " << inputSize << " bytes\n"
         << "Output Size: " << outputSize << " bytes\n"
         << fixed << setprecision(3)
         << "Size Ratio (Output/Input): " << sizeRatio << "\n"
         << "Time Taken: " << elapsedTime << " seconds\n";

    // Log stats if provided
    if (stats) {
        cout << "---------------------------------------------\n";
        cout << "Frame stats\n";
        cout << "Intra-frames: " << (*stats)[0] << "  "
             << "Inter-frames: " << (*stats)[1] << "\n"
             << "Intra Y-blocks: " << (*stats)[3] << "  "
             << "Inter Y-blocks: " << (*stats)[2] << "\n"
             << "Intra U-blocks: " << (*stats)[5] << "  "
             << "Inter U-blocks: " << (*stats)[4] << "\n"
             << "Intra V-blocks: " << (*stats)[7] << "  "
             << "Inter V-blocks: " << (*stats)[6] << "\n";
    }

    cout << "---------------------------------------------\n";
}
