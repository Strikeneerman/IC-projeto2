#include <iostream>
#include <fstream>
#include <vector>
#include "../Common/bitStream.h"
#include "../Common/golomb.h"
#include <opencv2/opencv.hpp>
#include <regex>

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
                int residual = golomb.decode(stream);  // This might throw if EOF is reached
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

void decodeRawVideo(const string& inputFile, const string& outputFile) {
    BitStream stream(inputFile, false);

    int width = stream.readBits(16);
    int height = stream.readBits(16);
    int m = stream.readBits(8);

    Golomb golomb(m, true);

    int frameSize = width * height;
    ofstream output(outputFile, ios::binary);


    Mat frame(height, width, CV_8UC1, Scalar(0));


    while (true) {
        if (stream.eof()) {
            cout << "EOF reached, exiting decoding loop." << endl;
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

    cout << "Raw video decoded successfully." << endl;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cout << "Usage:\n";
        cout << "./video -encode <input_raw_video> <output_encoded_file> <m>\n";
        cout << "./video -decode <input_encoded_file> <output_raw_video>\n";
        return 1;
    }

    string action = argv[1];

    if (action == "-encode" && argc == 5) {
        string inputFile = argv[2];
        string outputFile = argv[3];
        int m = stoi(argv[4]);

        encodeRawVideo(inputFile, outputFile, m);
    } else if (action == "-decode" && argc == 4) {
        string inputFile = argv[2];
        string outputFile = argv[3];

        decodeRawVideo(inputFile, outputFile);
    } else {
        cout << "Invalid arguments. Please use the following format:\n";
        cout << "./video -encode <input_raw_video> <output_encoded_file> <m>\n";
        cout << "./video -decode <input_encoded_file> <output_raw_video>\n";
    }

    return 0;
}
