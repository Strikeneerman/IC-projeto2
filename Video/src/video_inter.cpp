#include "VideoCodec.h"

int main(int argc, char** argv) {
    if (argc < 4) {
        cout << "Usage:\n";
        cout << "./video_frame -encode <input_raw_video> <output_encoded_file> [-s search_size] [-b block_size] [-f frames] [-l lossy_ratio]\n";
        cout << "./video_frame -decode <input_encoded_file> <output_raw_video>\n";
        return 1;
    }

    string action = argv[1];
    auto startTime = chrono::high_resolution_clock::now();

    if (action == "-encode") {
        string inputFile = argv[2];
        string outputFile = argv[3];

        // Default values
        int searchSize = 32;    // Default search area
        int blockSize = 4;      // Default block size
        int frames = -1;         // Default frames
        float lossyRatio = 1.0; // Default lossy ratio (1.0 means lossless)

        // Parse optional arguments
        for (int i = 4; i < argc; i += 2) {
            string param = argv[i];

            if (i + 1 >= argc) {
                cout << "Error: Missing value for parameter " << param << endl;
                return 1;
            }

            try {
                if (param == "-s" || param == "-search") {
                    searchSize = stoi(argv[i + 1]);
                }
                else if (param == "-b" || param == "-block") {
                    blockSize = stoi(argv[i + 1]);
                }
                else if (param == "-f" || param == "-frame") {
                    frames = stoi(argv[i + 1]);
                }
                else if (param == "-l" || param == "-lossy") {
                    lossyRatio = stof(argv[i + 1]);
                    if (lossyRatio <= 0.0 || lossyRatio > 1.0) {
                        cout << "Error: Lossy ratio must be between 0.0 and 1.0" << endl;
                        return 1;
                    }
                }
                else {
                    cout << "Unknown parameter: " << param << endl;
                    return 1;
                }
            }
            catch (const std::invalid_argument&) {
                cout << "Error: Invalid number format for parameter " << param << endl;
                return 1;
            }
            catch (const std::out_of_range&) {
                cout << "Error: Number out of range for parameter " << param << endl;
                return 1;
            }
        }

        BlockMatchingParams params = BlockMatchingParams(searchSize, blockSize);
        encodeRawVideo(inputFile, outputFile, params, frames);

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
        cout << "./video_frame -encode <input_raw_video> <output_encoded_file> [-s search_size] [-b block_size] [-f frames] [-l lossy_ratio]\n";
        cout << "./video_frame -decode <input_encoded_file> <output_raw_video>\n";
    }

    return 0;
}