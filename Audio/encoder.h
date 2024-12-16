#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "../Common/golomb.h"
#include "./SFML-2.6.2/include/SFML/Audio.hpp"
#include "./audio_utilities.h"

int encode(std::string file_path, std::string compression_type, int target_bitrate, int taylor_degree) {
    const int frame_size = 32000;
    const bool useInterleaving = false;
    const int max_q_bits = 12;
    const int bitrate_margin = 5;     // Acceptable error from target bitrate
    const int max_taylor_degree = 7;  // Acceptable error from target bitrate

    int q_bits = compression_type == "lossless" ? 0 : 4;  // Quantization factor in bits
    bool iterate_over_predictors;

    if (taylor_degree == -1) {
        iterate_over_predictors = true;
        taylor_degree = 0;
    }

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

    // Open destination file
    BitStream stream("./outputs/encoded_audio/" + std::filesystem::path(file_path).stem().string() + ".g7a", true);
    writeHeader(stream, channelCount, buffer.getSampleRate(), frame_size, (uint32_t)buffer.getSampleCount(), useInterleaving);

    // Open a CSV file to log the taylor degrees used
    std::ofstream csvFile;
    csvFile.open("taylor_degrees.csv");

    // Iterate through samples frame by frame
    for (uint32_t frameStart = 0; frameStart < sampleCount; frameStart += frame_size) {
        // Determine the current frame size (might be smaller for the last frame)
        int currentFrameSize = std::min(frame_size, (int)(sampleCount - frameStart));
        int frame_taylor_degree = taylor_degree == -1 ? 0 : taylor_degree;

        // Keep track of samples and residuals for different taylor degrees
        std::vector<std::vector<sf::Int16>> vector_frameSamples;
        std::vector<std::vector<int>> vector_frameResiduals;
        std::vector<double> entropy;
        for (int i = 0; i <= max_taylor_degree; i++) {
            vector_frameSamples.emplace_back();
            vector_frameResiduals.emplace_back();
        }

        // Apply the predictor and compute residuals for each degree
        do {
            vector_frameSamples[frame_taylor_degree].reserve(currentFrameSize);
            vector_frameResiduals[frame_taylor_degree].reserve(currentFrameSize);
            for (int i = 0; i < currentFrameSize; ++i) {
                int predicted = predictor_taylor(frame_taylor_degree, channelCount, vector_frameSamples[frame_taylor_degree]);
                int residual = (globalSamples[frameStart + i] - predicted);
                residual = residual >> q_bits;
                vector_frameResiduals[frame_taylor_degree].push_back(residual);
                vector_frameSamples[frame_taylor_degree].push_back(predicted + (residual << q_bits));
            }
            entropy.push_back(get_entropy(vector_frameResiduals[frame_taylor_degree]));
            cout << ' ' << get_entropy(vector_frameResiduals[frame_taylor_degree]) << ' ';
            frame_taylor_degree++;
        } while (iterate_over_predictors && frame_taylor_degree <= max_taylor_degree);

        // Find the taylor_degree that minimizes entropy
        double min_entropy = 1000;
        int min_entropy_degree = -1;
        for(int i = 0; i < entropy.size(); i++){
            if(entropy[i] < min_entropy){
                min_entropy = entropy[i];
                min_entropy_degree = i;
            }
        }
        cout << "Taylor degree with least entropy: " << min_entropy_degree << endl;

        // Calculate optimal Golomb M
        int sum_values = std::accumulate(vector_frameResiduals[min_entropy_degree].begin(), vector_frameResiduals[min_entropy_degree].end(),
                                         0, [](int acc, int val) { return acc + std::abs(val); });
        double average = static_cast<double>(sum_values) / vector_frameResiduals[min_entropy_degree].size();
        if (useInterleaving) average *= 2;
        int m = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (average + 1)))));
        if (m <= 1) m = 2;

        csvFile << min_entropy_degree << '\n';
        // cout << "Encoding frame starting at sample " << frameStart << " with size " << currentFrameSize << endl;
        // cout << " Golomb M: " << m << " Average residual: " << average << " Q_bits: " << q_bits << endl;

        // Write frame header
        stream.writeBits(m, 16);
        stream.writeBits(q_bits, 4);
        stream.writeBits(min_entropy_degree, 3);

        // Write the residuals to the file
        Golomb golomb(m, useInterleaving);
        int bits_written = 0;
        for (int i = 0; i < currentFrameSize; ++i) {
            bits_written += golomb.encode(stream, vector_frameResiduals[min_entropy_degree][i]);
        }

        // Calculate bitrate used and adapt quantization if needed
        if (compression_type == "lossy") {
            double frame_time = (double)currentFrameSize / (buffer.getSampleRate() * channelCount);
            double bitrate = (double)bits_written / (frame_time * 1000);  // in kbps
            if (bitrate < target_bitrate + bitrate_margin && q_bits > 0)
                q_bits--;
            else if (bitrate > target_bitrate - bitrate_margin && q_bits < max_q_bits)
                q_bits++;
            cout << "Frame bitrate (kbps): " << bitrate << " Target: " << target_bitrate << " Q_bits: " << q_bits << endl;
        }
    }
    csvFile.close();
    return 0;
}
