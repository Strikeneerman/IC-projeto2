#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "../Common/golomb.h"
#include "./SFML-2.6.2/include/SFML/Audio.hpp"
#include "./audio_utilities.h"

int encode(std::string file_path, std::string compression_type, int target_bitrate) {
    const int frame_size = 128;
    const int taylor_degree = 0;
    const bool useInterleaving = false;

    const int bitrate_margin = 5;   // Acceptable error from target bitrate
    int q_bits = compression_type == "lossless" ? 0 : 4;    // Quantization factor in bits

    // Load source file
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(file_path)) {
        std::cerr << "Failed to load WAV file: " << file_path << std::endl;
        return 1;
    }
    unsigned int channelCount = buffer.getChannelCount();
    const int sampleCount = buffer.getSampleCount();
    const sf::Int16 *samples = buffer.getSamples();
    const std::vector<sf::Int16> originalSampleVector(samples, samples + sampleCount);

    // Open destination file
    BitStream stream("./outputs/encoded_audio/" + std::filesystem::path(file_path).stem().string() + ".g7a", true);
    writeHeader(stream, channelCount, buffer.getSampleRate(), frame_size, buffer.getSampleCount(), taylor_degree, useInterleaving);

    // Iterate through samples frame by frame
    for (int frameStart = 0; frameStart < sampleCount; frameStart += frame_size) {
        // Determine the current frame size (might be smaller for the last frame)
        int currentFrameSize = std::min(frame_size, sampleCount - frameStart);
        std::vector<std::vector<sf::Int16>> samples(channelCount);

        // Apply the predictor and compute residuals
        std::vector<std::vector<sf::Int16>> residuals(channelCount);
        for (int i = 0; i < currentFrameSize; ++i) {
            int globalIndex = frameStart + i;
            int channelIndex = globalIndex % channelCount;

            std::vector<sf::Int16> &currentResiduals = residuals[channelIndex];
            std::vector<sf::Int16> &currentChannel = samples[channelIndex];
            currentChannel.push_back(originalSampleVector[globalIndex]); // build a vetcor for each channel
            
            sf::Int16 residual;
            sf::Int16 predicted;
            
            if (i <= taylor_degree) {
                residual = currentChannel[i];
            } else {
                predicted = predictor_taylor(taylor_degree, &currentChannel[i - 1]);
                residual = currentChannel[i] - predicted;
            }

            // If we are dealing with lossy compression, quantize here and update the channels vector
            if(compression_type == "lossy"){
                residual = residual >> q_bits;
                currentChannel[i] = residual << q_bits + predicted;
            }
            currentResiduals.push_back(residual);
            //saveHistogram(currentResiduals, "residuals_taylor_" + std::to_string(taylor_degree), 64);
        }

        // Calculate optimal Golomb M
        int sum_values = 0;
        for (int channel = 0; channel < channelCount; channel++) {
            sum_values += std::accumulate(
                residuals[channel].begin(), 
                residuals[channel].end(), 
                0, 
                [](int acc, int val) { return acc + std::abs(val); }
            );
        }
        double average = static_cast<double>(sum_values) / (residuals[0].size() * channelCount);
        cout << "Average residual: " << average << endl;
        if(useInterleaving) average *= 2;
        double p = 1 / (average + 1);
        int m = static_cast<int>(std::ceil(-1 / std::log2(1 - p)));
        if(m <= 1) m = 2;

        cout << "Optimal m: " << m << endl;

        stream.writeBits(m, 12);        // Write m with 12 bit precision (m up to 4096)
        stream.writeBits(q_bits, 4);    // Write the quantization factor at the satrt of the frame as well

        // Write the residuals to the file
        Golomb golomb(m, useInterleaving);
        int bits_written = 0;
        for (int i = 0; i < currentFrameSize; ++i) {
            int globalIndex = frameStart + i;
            int channelIndex = globalIndex % channelCount;
            const std::vector<sf::Int16> &currentResiduals = residuals[channelIndex];
            bits_written += golomb.encode(stream, currentResiduals[i]);
            
            //std::vector<sf::Int16> &currentChannel = samples[channelIndex];
            //cout << "Sample: " << currentChannel[i] << " Predicted: " << currentChannel[i] - currentResiduals[i] << " Residual: " << currentResiduals[i] << endl;
        }

        // Calculate bitrate used and adapt quantization if needed
        if(compression_type == "lossy"){
            double frame_time = currentFrameSize/(buffer.getSampleRate() * channelCount);
            double bitrate = bits_written/(frame_time * 1000); // in kbps
            if(bitrate > target_bitrate + bitrate_margin) q_bits--; 
            else if(bitrate < target_bitrate - bitrate_margin) q_bits++;
        }
    }

    return 0;
}

// To do:
// Try to simplify the code by spearating things into functions
// Make decoder
// Test
