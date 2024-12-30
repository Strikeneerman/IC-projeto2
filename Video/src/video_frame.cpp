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
void encodeFrame(const Mat& frame, Golomb& golomb, BitStream& stream) {
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            int predicted = predictPixel(frame, x, y);
            int residual = frame.at<uchar>(y, x) - predicted;
            golomb.encode(stream, residual);
        }
    }
}

// Decoding a single frame (Y channel only)
void decodeFrame(Mat& frame, Golomb& golomb, BitStream& stream) {
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

// Encode raw YUV video
void encodeRawVideo(const string& inputFile, const string& outputFile, int m) {
    ifstream input(inputFile, ios::binary);
    if (!input) {
        cerr << "Error: Could not open input file." << endl;
        return;
    }

    int width, height;
    parseY4MHeader(input, width, height);

    BitStream stream(outputFile, true);
    Golomb golomb(m, true);

    stream.writeBits(width, 16);
    stream.writeBits(height, 16);
    stream.writeBits(m, 8);

    int frameSize = width * height;
    Mat frame(height, width, CV_8UC1);

    while (input.read(reinterpret_cast<char*>(frame.data), frameSize)) {
        encodeFrame(frame, golomb, stream);
    }

    cout << "Raw video encoded successfully." << endl;
}

// Decode raw YUV video and write Y4M header
void decodeRawVideo(const string& inputFile, const string& outputFile) {
    BitStream stream(inputFile, false);

    int width = stream.readBits(16);
    int height = stream.readBits(16);
    int m = stream.readBits(8);

    Golomb golomb(m, true);

    int frameSize = width * height;
    ofstream output(outputFile, ios::binary);

    // Write Y4M header
    output << "YUV4MPEG2 W" << width << " H" << height << " F30:1 Ip A0:0\n";

    Mat frame(height, width, CV_8UC1, Scalar(0));

    while (true) {
        if (stream.eof()) {
           
            break;
        }
        try {
            decodeFrame(frame, golomb, stream);
            output.write(reinterpret_cast<const char*>(frame.data), frameSize);
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


int main(int argc, char** argv) {
    if (argc < 4) {
        cout << "Usage:\n";
        cout << "./video_frame -encode <input_raw_video> <output_encoded_file> <m>\n";
        cout << "./video_frame -decode <input_encoded_file> <output_raw_video>\n";
        return 1;
    }

    string action = argv[1];
    auto startTime = chrono::high_resolution_clock::now();

    if (action == "-encode" && argc == 5) {
        string inputFile = argv[2];
        string outputFile = argv[3];
        int m = stoi(argv[4]);

        encodeRawVideo(inputFile, outputFile, m);
        
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
        cout << "./video_frame -encode <input_raw_video> <output_encoded_file> <m>\n";
        cout << "./video_frame -decode <input_encoded_file> <output_raw_video>\n";
    }

    return 0;
}
