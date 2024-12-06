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
    int8_t channelCount;
    int16_t samplingFreq;
    int16_t frame_size;
    int32_t totalSamples;
    int8_t taylor_degree;
    bool useInterleaving;
    
    readHeader(stream, channelCount, samplingFreq, frame_size, totalSamples, taylor_degree, useInterleaving);

    std::cout << "Channel Count: " << static_cast<int>(channelCount) << '\n';
    std::cout << "Sampling Frequency: " << samplingFreq << " Hz\n";
    std::cout << "Frame Size: " << frame_size << '\n';
    std::cout << "Total Samples: " << totalSamples << '\n';
    std::cout << "Taylor Degree: " << static_cast<int>(taylor_degree) << '\n';
    std::cout << "Use Interleaving: " << (useInterleaving ? "Yes" : "No") << '\n';

    // Prepare output vector
    std::vector<sf::Int16> reconstructedSamples;
    reconstructedSamples.reserve(totalSamples);

    // Iterate through frames
    for (int frameStart = 0; frameStart < totalSamples; frameStart += frame_size) {
        
        // Determine current frame size
        int currentFrameSize = std::min((int)frame_size, totalSamples - frameStart);

        cout << "Decoding frame starting at sample " << frameStart << " with size " << currentFrameSize << endl; 
        
        // Read frame header
        int m = stream.readBits(12);        // Read Golomb m parameter
        int q_bits = stream.readBits(4);    // Read quantization factor
        
        cout << " Golomb M: " << m << " Q_bits: " << q_bits << endl;

        // Initialize Golomb decoder
        Golomb golomb(m, useInterleaving);
        
        // Prepare channel containers
        std::vector<std::vector<sf::Int16>> channels(channelCount);
        
        // Decode frame
        for (int i = 0; i < currentFrameSize; ++i) {
            int globalIndex = frameStart + i;
            int channelIndex = globalIndex % channelCount;
            
            std::vector<sf::Int16> &currentChannel = channels[channelIndex];
            
            // Decode residual for current channel
            int residual = golomb.decode(stream);
            
            // De-quantize if needed
            if (q_bits > 0) {
                residual = residual << q_bits;
            }
            
            // Predict based on previous samples
            sf::Int16 predicted;
            if (channelIndex <= taylor_degree) {
                predicted = currentChannel.empty() ? 0 : currentChannel.back();
            } else {
                predicted = predictor_taylor(taylor_degree, &currentChannel[currentChannel.size() - 1]);
            }
            
            // Reconstruct sample
            sf::Int16 reconstructedSample = predicted + residual;
            currentChannel.push_back(reconstructedSample);
            reconstructedSamples.push_back(reconstructedSample);

            //cout << "Residual: " << residual << " Predicted: " << predicted << " Reconstructed Sample: " << reconstructedSample << endl;

        }
    }

    // Save reconstructed audio as WAV
    std::string outputFilename = std::filesystem::path(file_path).stem().string() + "_decoded.wav";
    saveWav(reconstructedSamples, samplingFreq, channelCount, outputFilename);
    return 0;
}