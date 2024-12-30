#include <iostream>
#include <fstream>
#include <vector>
#include "../../Common/bitStream.h"
#include "../../Common/golomb.h"
#include <opencv2/opencv.hpp>
#include <regex>

//logging imports
#include <chrono>
#include <fstream>
#include <iomanip>


using namespace std;
using namespace cv;

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

int computeOptimalM(const Mat& frame) {
    double meanResidual = 0;
    int pixelCount = frame.rows * frame.cols;

    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            int predicted = predictPixel(frame, x, y);
            int residual = frame.at<uchar>(y, x) - predicted;
            meanResidual += abs(residual);
        }
    }

    meanResidual /= pixelCount;
    int optimalM = max(2, static_cast<int>(ceil(-1 / log2(meanResidual / 256.0))));
    return min(16, optimalM); // Clamp to a maximum of 16
}

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

void decodeFrameBlockBased(Mat& frame, BitStream& stream, int staticM) {
    const int blockSize = 16; // Block size (16x16)
    for (int by = 0; by < frame.rows; by += blockSize) {
        for (int bx = 0; bx < frame.cols; bx += blockSize) {
            Rect blockRect(bx, by, min(blockSize, frame.cols - bx), min(blockSize, frame.rows - by));
            Mat block = frame(blockRect);
            
            int m = staticM;
            if(m == -1) {
                m = stream.readBits(4) + 2;
            }
            
            // Ensure m is valid
            if(m < 2) m = 2;
            if(m > 16) m = 16;
            Golomb golomb(m, true);

            for (int y = 0; y < block.rows; ++y) {
                for (int x = 0; x < block.cols; ++x) {
                    try {
                        int residual = golomb.decode(stream);
                        int predicted = predictPixel(block, x, y);
                        block.at<uchar>(y, x) = saturate_cast<uchar>(residual + predicted);
                    } catch (const std::runtime_error& e) {
                        cerr << "Error during decoding: " << e.what() << endl;
                        return;
                    }
                }
            }
        }
    }
}

void encodeFrameBlockBased(const Mat& frame, BitStream& stream, int staticM) {
    const int blockSize = 16; // Block size (16x16)
    for (int by = 0; by < frame.rows; by += blockSize) {
        for (int bx = 0; bx < frame.cols; bx += blockSize) {
            Rect blockRect(bx, by, min(blockSize, frame.cols - bx), min(blockSize, frame.rows - by));
            Mat block = frame(blockRect);

            int m = staticM;
            if(m == -1){ 
                m = computeOptimalM(block);
                stream.writeBits(m - 2, 4);  // Encode `m`
            }
            
            // Ensure m is valid before creating Golomb
            if(m < 2) m = 2;
            if(m > 16) m = 16;
            Golomb golomb(m, true);
            

            for (int y = 0; y < block.rows; ++y) {
                for (int x = 0; x < block.cols; ++x) {
                    int predicted = predictPixel(block, x, y);
                    int residual = block.at<uchar>(y, x) - predicted;
                    golomb.encode(stream, residual);
                }
            }
        }
    }
}


void encodeRawVideo(const string& inputFile, const string& outputFile, int staticM) {
    ifstream input(inputFile, ios::binary);
    if (!input) {
        cerr << "Error: Could not open input file." << endl;
        return;
    }

    int width, height;
    parseY4MHeader(input, width, height);

    BitStream stream(outputFile, true);

    stream.writeBits(width, 16);
    stream.writeBits(height, 16);

    bool isStaticM = staticM > 0;
    stream.writeBits(isStaticM ? 1 : 0, 1);

    // If static `m`, write it to the header
    if (isStaticM) {
        cout << staticM;
        stream.writeBits(staticM - 2, 4); // Store static m
    }

    int frameSize = width * height;
    Mat frame(height, width, CV_8UC1);

    while (input.read(reinterpret_cast<char*>(frame.data), frameSize)) {
        encodeFrameBlockBased(frame, stream, staticM);
    }

    cout << "Raw video encoded successfully with block-based processing." << endl;
}


void decodeRawVideo(const string& inputFile, const string& outputFile) {
    BitStream stream(inputFile, false);

    int width = stream.readBits(16);
    int height = stream.readBits(16);

    bool isStaticM = stream.readBits(1);
    int staticM = isStaticM ? stream.readBits(4) + 2 : -1;
    
    int frameSize = width * height;
    ofstream output(outputFile, ios::binary);

    // Write Y4M header
    output << "YUV4MPEG2 W" << width << " H" << height << " F30:1 Ip A0:0\n";

    Mat frame(height, width, CV_8UC1);

    while (true) {
        if (stream.eof()) {
            break;
        }
        try {
            decodeFrameBlockBased(frame, stream, staticM);
            output.write(reinterpret_cast<const char*>(frame.data), frameSize);
        } catch (const std::runtime_error& e) {
            cerr << "Decoding finished or encountered error: " << e.what() << endl;
            break;
        }
    }

 
}



void logResults(const string& logFile, const string& inputFile, const string& outputFile, 
                const string& operation, bool verbose, double elapsedTime) {
    ifstream in(inputFile, ios::binary | ios::ate);
    ifstream out(outputFile, ios::binary | ios::ate);

    if (!in || !out) {
        cerr << "Error: Could not open input or output file to log results." << endl;
        return;
    }

    auto inputSize = in.tellg();
    auto outputSize = out.tellg();

    double sizeRatio = static_cast<double>(outputSize) / static_cast<double>(inputSize);

    // Prepare the log message
    stringstream logMessage;
    logMessage << "Operation: " << operation << "\n"
               << "Input File: " << inputFile << "\n"
               << "Output File: " << outputFile << "\n"
               << "Input Size: " << inputSize << " bytes\n"
               << "Output Size: " << outputSize << " bytes\n"
               << fixed << setprecision(3)
               << "Size Ratio (Output/Input): " << sizeRatio << "\n"
               << "Time Taken: " << elapsedTime << " seconds\n"
               << "---------------------------------------------\n";

    // Write to log file
    if (!logFile.empty()) {
        ofstream logStream(logFile, ios::app);
        if (logStream) {
            logStream << logMessage.str();
        } else {
            cerr << "Error: Could not write to log file." << endl;
        }
    }

    // Print to console if verbose
    if (verbose) {
        cout << logMessage.str();
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cout << "Usage:\n";
        cout << "./video -encode <input_raw_video> <output_encoded_file> [-m <value>] [-l <log_file>] [-v]\n";
        cout << "./video -decode <input_encoded_file> <output_raw_video> [-l <log_file>] [-v]\n";
        return 1;
    }

    string action = argv[1];
    string inputFile = argv[2];
    string outputFile = argv[3];

    string logFile;
    bool verbose = false;
    int staticM = -1;

    // Parse optional arguments
    for (int i = 4; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-l" && i + 1 < argc) {
            logFile = argv[++i];
        } else if (arg == "-v") {
            verbose = true;
        } else if (arg == "-m" && i + 1 < argc) {
            staticM = stoi(argv[++i]);
            if (staticM < 2 || staticM > 16) {
                cerr << "Error: m must be between 2 and 16." << endl;
                return 1;
            }
        } else {
            cerr << "Invalid argument: " << arg << endl;
            return 1;
        }
    }

    auto startTime = chrono::high_resolution_clock::now();

    if (action == "-encode") {
        encodeRawVideo(inputFile, outputFile, staticM);
    } else if (action == "-decode") {
        decodeRawVideo(inputFile, outputFile);
    } else {
        cout << "Invalid action. Please use the following format:\n";
        cout << "./video -encode <input_raw_video> <output_encoded_file>\n";
        cout << "./video -decode <input_encoded_file> <output_raw_video>\n";
        return 1;
    }

    auto endTime = chrono::high_resolution_clock::now();
    double elapsedTime = chrono::duration<double>(endTime - startTime).count();

    string operation = (action == "-encode") ? "EncodingBlocks" : "DecodingBlocks";
    logResults(logFile, inputFile, outputFile, operation, verbose, elapsedTime);

    return 0;
}
