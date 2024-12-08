#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "../Common/golomb.h"
#include "./SFML-2.6.2/include/SFML/Audio.hpp"
#include "./audio_utilities.h"

int decode(std::string file_path) {
    // Open input compressed file
    BitStream stream(file_path, false);

    // Read header information
    uint8_t channelCount;
    uint16_t samplingFreq;
    uint16_t frame_size;
    uint32_t totalSamples;
    uint8_t taylor_degree;
    bool useInterleaving;

    readHeader(stream, channelCount, samplingFreq, frame_size, totalSamples, taylor_degree, useInterleaving);

    std::cout << "Channel Count: " << static_cast<int>(channelCount) << '\n';
    std::cout << "Sampling Frequency: " << samplingFreq << " Hz\n";
    std::cout << "Frame Size: " << frame_size << '\n';
    std::cout << "Total Samples: " << totalSamples << '\n';
    std::cout << "Taylor Degree: " << static_cast<int>(taylor_degree) << '\n';
    std::cout << "Use Interleaving: " << (useInterleaving ? "Yes" : "No") << '\n';

    // Prepare output vector
    std::vector<sf::Int16> globalSamples;
    globalSamples.reserve(totalSamples);

    // Iterate through frames
    for (int frameStart = 0; frameStart < totalSamples; frameStart += frame_size) {
        int currentFrameSize = std::min((int)frame_size, (int)(totalSamples - frameStart));
        std::vector<sf::Int16> frameSamples;
        frameSamples.reserve(currentFrameSize);

        // Read frame header
        int m = stream.readBits(16);      // Read Golomb m parameter
        int q_bits = stream.readBits(4);  // Read quantization factor

        // cout << "Decoding frame starting at sample " << frameStart << " with size " << currentFrameSize << endl;
        // cout << " Golomb M: " << m << " Q_bits: " << q_bits << endl;

        // Initialize Golomb decoder
        Golomb golomb(m, useInterleaving);
        for (int i = 0; i < currentFrameSize; ++i) {
            int residual = golomb.decode(stream) << q_bits;

            // int predicted = predictor_basic(frameSamples);
            int predicted = predictor_taylor(taylor_degree, channelCount, frameSamples);

            sf::Int16 reconstructedSample = predicted + residual;
            globalSamples.push_back(reconstructedSample);
            frameSamples.push_back(reconstructedSample);
            // cout << "Residual: " << residual << " Predicted: " << predicted << " Reconstructed Sample: " << reconstructedSample << endl;
        }
    }

    // Save reconstructed audio as WAV
    std::string outputFilename = std::filesystem::path(file_path).stem().string() + "_decoded.wav";
    saveWav(globalSamples, samplingFreq, channelCount, outputFilename);
    return 0;
}