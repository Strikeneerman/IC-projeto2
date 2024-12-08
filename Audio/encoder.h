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
    const int taylor_degree = 2;
    const bool useInterleaving = false;

    const int bitrate_margin = 5;                         // Acceptable error from target bitrate
    int q_bits = compression_type == "lossless" ? 0 : 4;  // Quantization factor in bits

    // Load source file
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(file_path)) {
        std::cerr << "Failed to load WAV file: " << file_path << std::endl;
        return 1;
    }
    printAudioInfo(buffer);

    unsigned int channelCount = buffer.getChannelCount();
    const uint32_t sampleCount = buffer.getSampleCount();
    const sf::Int16 *samples = buffer.getSamples();
    const std::vector<sf::Int16> globalSamples(samples, samples + sampleCount);

    std::vector<int> globalResiduals;
    globalResiduals.reserve(sampleCount);

    // Open destination file
    BitStream stream("./outputs/encoded_audio/" + std::filesystem::path(file_path).stem().string() + ".g7a", true);
    writeHeader(stream, channelCount, buffer.getSampleRate(), frame_size, (uint32_t)buffer.getSampleCount(), taylor_degree,
                useInterleaving);

    // Iterate through samples frame by frame
    for (uint32_t frameStart = 0; frameStart < sampleCount; frameStart += frame_size) {
        // Determine the current frame size (might be smaller for the last frame)
        int currentFrameSize = std::min(frame_size, (int)(sampleCount - frameStart));
        std::vector<int> frameResiduals;
        std::vector<sf::Int16> frameSamples;
        frameResiduals.reserve(currentFrameSize);
        frameSamples.reserve(currentFrameSize);

        // Apply the predictor and compute residuals
        for (int i = 0; i < currentFrameSize; ++i) {
            int predicted;
            int residual;

            predicted = predictor_taylor(taylor_degree, channelCount, frameSamples);
            residual = (globalSamples[frameStart + i] - predicted) >> q_bits;

            // cout << "Residual: " << residual << " Predicted: " << predicted << " Reconstructed Sample: " << (predicted + residual <<
            // q_bits) << " Real Sample: " << globalSamples[frameStart + i] << endl;

            globalResiduals.push_back(residual);
            frameResiduals.push_back(residual);
            frameSamples.push_back(predicted + residual << q_bits);
        }

        // Calculate optimal Golomb M
        int sum_values =
            std::accumulate(frameResiduals.begin(), frameResiduals.end(), 0, [](int acc, int val) { return acc + std::abs(val); });
        double average = static_cast<double>(sum_values) / frameResiduals.size();
        if (useInterleaving) average *= 2;
        double p = 1 / (average + 1);
        int m = static_cast<int>(std::ceil(-1 / std::log2(1 - p)));
        if (m <= 1) m = 2;

        // cout << "Encoding frame starting at sample " << frameStart << " with size " << currentFrameSize << endl;
        // cout << " Golomb M: " << m << " Q_bits: " << q_bits << endl;
        // cout << "Average residual: " << average << endl;

        stream.writeBits(m, 16);      // Write m with 16 bit precision
        stream.writeBits(q_bits, 4);  // Write the quantization factor at the satrt of the frame as well

        // Write the residuals to the file
        Golomb golomb(m, useInterleaving);
        int bits_written = 0;
        for (int i = 0; i < currentFrameSize; ++i) {
            bits_written += golomb.encode(stream, frameResiduals[i]);
        }

        // Calculate bitrate used and adapt quantization if needed
        if (compression_type == "lossy") {
            double frame_time = currentFrameSize / (buffer.getSampleRate() * channelCount);
            double bitrate = bits_written / (frame_time * 1000);  // in kbps
            if (bitrate > target_bitrate + bitrate_margin)
                q_bits--;
            else if (bitrate < target_bitrate - bitrate_margin)
                q_bits++;
        }
    }

    return 0;
}
