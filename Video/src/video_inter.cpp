#include "VideoCodec.h"

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

        BlockMatchingParams params = BlockMatchingParams(32, 4);
        encodeRawVideo(inputFile, outputFile, params);

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