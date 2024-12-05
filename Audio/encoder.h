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
    const int frame_size = 1024;
    const int taylor_degree = 3;
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
    const std::vector<sf::Int16> sampleVector(samples, samples + sampleCount);

    // Open destination file
    BitStream stream(std::filesystem::path(file_path).stem().string() + ".g7a", true);
    writeHeader(stream, channelCount, buffer.getSampleRate(), frame_size, buffer.getSampleCount());

    // Iterate through samples frame by frame
    for (int frameStart = 0; frameStart < sampleCount; frameStart += frame_size) {
        // Determine the current frame size (might be smaller for the last frame)
        int currentFrameSize = std::min(frame_size, sampleCount - frameStart);
        std::vector<std::vector<sf::Int16>> channels(channelCount);

        // Apply the predictor and compute residuals
        std::vector<std::vector<sf::Int16>> residuals(channelCount);
        for (int i = 0; i < currentFrameSize; ++i) {
            int globalIndex = frameStart + i;
            int channelIndex = globalIndex % channelCount;

            std::vector<sf::Int16> &currentResiduals = residuals[channelIndex];
            std::vector<sf::Int16> &currentChannel = channels[channelIndex];
            currentChannel.push_back(sampleVector[globalIndex]); // build a vetcor for each channel
            
            sf::Int16 residual;
            sf::Int16 predicted;
            
            if (channelIndex <= taylor_degree) {
                residual = currentChannel[channelIndex];
            } else {
                predicted = predictor_taylor(taylor_degree, &currentChannel[channelIndex - 1]);
                residual = currentChannel[channelIndex] - predicted;
            }

            // If we are dealing with lossy, quantize here and update the channels vector
            if(compression_type == "lossy"){
                residual = residual >> q_bits;
                currentChannel[channelIndex] = residual << q_bits + predicted;
            }
            currentResiduals.push_back(residual);
            //saveHistogram(currentResiduals, "residuals_taylor_" + std::to_string(taylor_degree), 64);
        }

        // Calculate optimal Golomb M
        int sum_values = 0;
        for(int channel = 0; channel < channelCount; channel++){
            sum_values += std::accumulate(residuals[channel].begin(), residuals[channel].end(), 0);
        }
        double average = static_cast<double>(sum_values) / (residuals[0].size() * channelCount);
        double p = 1 / (average + 1);
        int m = static_cast<int>(std::ceil(-1 / std::log2(1 - p)));

        stream.writeBits(m, 12);        // Write m with 12 bit precision (m up to 4096)
        stream.writeBits(q_bits, 4);    // Write the quantization factor at the satrt of the frame as well

        // Write the residuals to the file
        Golomb golomb(m, useInterleaving);
        int bits_written = 0;
        for (int i = 0; i < currentFrameSize; ++i) {
            int globalIndex = frameStart + i;
            int channelIndex = globalIndex % channelCount;
            const std::vector<sf::Int16> &currentResiduals = residuals[channelIndex];
            bits_written += golomb.encode(stream, currentResiduals[channelIndex]);
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



/*

// =============== APPLY PREDICTOR AND CALCULATE RESIDUALS ==========================
    
    std::vector<std::vector<sf::Int16>> residuals(channelCount);

    for (unsigned int channel = 0; channel < channelCount; ++channel) {
        const std::vector<sf::Int16> &currentChannel = channels[channel];
        std::vector<sf::Int16> &currentResiduals = residuals[channel];
        currentResiduals.reserve(currentChannel.size());

        for (int i = 0; i < currentChannel.size(); ++i) {
        if (i <= taylor_degree) {
            currentResiduals.push_back(currentChannel[i]);
        } else {
            sf::Int16 predicted = predictor_taylor(taylor_degree, &currentChannel[i - 1]);
            sf::Int16 residual = currentChannel[i] - predicted;
            currentResiduals.push_back(residual);
        }
        }
        saveHistogram(currentResiduals, "residuals_taylor_" + std::to_string(taylor_degree), 64);
    }

    // =============== WRITE A HEADER WITH METADATA =================
    BitStream stream(std::filesystem::path(file_path).stem().string() + ".g7a", true);
    int frame_size = 1024;
    writeHeader(stream, channelCount, buffer.getSampleRate(), frame_size, buffer.getSampleCount());

        // =============== CALCULATE OPTIMAL GOLOMB M ===================
    int sum = std::accumulate(residuals[0].begin(), residuals[0].end(), 0);
    double average = static_cast<double>(sum) / residuals[0].size();
    double p = 1 / (average + 1);
    int m = static_cast<int>(std::ceil(-1 / std::log2(1 - p)));
    stream.writeBits(m, 12);  // Write m with 12 bit precision (m up to 4096)
*/