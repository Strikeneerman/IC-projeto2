#ifndef AUDIO_UTILITIES
#define AUDIO_UTILITIES

#include <cmath>
#include <unordered_map>

void printAudioInfo(const sf::SoundBuffer &buffer) {
    std::cout << "Audio File Information:" << std::endl;
    std::cout << "Sample Rate: " << buffer.getSampleRate() << " Hz" << std::endl;
    std::cout << "Channel Count: " << buffer.getChannelCount() << std::endl;
    std::cout << "Duration: " << buffer.getDuration().asSeconds() << " seconds" << std::endl;
    std::cout << "Sample Count: " << buffer.getSampleCount() << std::endl;
    std::cout << "Sample Size: " << sizeof(sf::Int16) * 8 << " bits" << std::endl;
}

void saveWav(const std::vector<sf::Int16> &samples, unsigned int sampleRate, unsigned int channelCount, const std::string &filename) {
    sf::SoundBuffer buffer;
    buffer.loadFromSamples(samples.data(), samples.size(), channelCount, sampleRate);
    std::string output_directory = "./outputs/wav_audio/";
    std::filesystem::create_directory("./outputs/");
    std::filesystem::create_directory(output_directory);
    if (!buffer.saveToFile(output_directory + filename)) {
        std::cerr << "Failed to save WAV file: " << filename << std::endl;
    }
}

void saveHistogram(const std::vector<sf::Int16> &data, const std::string &title, int num_bins) {
    auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());
    double min_val = *min_it;
    double max_val = *max_it;

    std::vector<int> bins(num_bins, 0);
    double bin_width = (max_val - min_val) / num_bins;

    for (double value : data) {
        int bin = static_cast<int>((value - min_val) / bin_width);
        if (bin == num_bins) bin--;
        bins[bin]++;
    }

    std::vector<double> bin_centers(num_bins);
    for (int i = 0; i < num_bins; i++) {
        bin_centers[i] = min_val + (i + 0.5) * bin_width;
    }

    std::string output_directory = "./outputs/histograms/";
    std::filesystem::create_directory("./outputs/");
    std::filesystem::create_directory(output_directory);

    std::string filename = output_directory + title + ".csv";
    std::ofstream outfile(filename);
    outfile << std::setprecision(10);
    outfile << "Bin Center,Frequency\n";
    for (size_t i = 0; i < bins.size(); ++i) {
        outfile << bin_centers[i] << "," << bins[i] << "\n";
    }
    outfile.close();
}

sf::Int16 predictor_basic(const std::vector<sf::Int16> &recentSamples) { return recentSamples[recentSamples.size()]; }

sf::Int16 predictor_taylor(int degree, int channelCount, const std::vector<sf::Int16> &recentSamples) {
    if (recentSamples.size() / channelCount == 0) {
        return 0;  // No recent samples, return 0
    }
    if (degree >= recentSamples.size() / channelCount) {
        return recentSamples[recentSamples.size() - channelCount];  // Not enough samples for a prediction, return last sample
    }
    // Precompute factorial values
    std::vector<int> factorial(degree + 1);
    factorial[0] = 1;
    for (int i = 1; i <= degree; ++i) {
        factorial[i] = factorial[i - 1] * i;
    }
    // Compute Taylor terms approximating derivatives using finite differences (backward Euler method)
    float predicted = recentSamples[recentSamples.size() - channelCount];
    for (int n = 1; n <= degree; ++n) {
        float nth_term = 0;
        for (int i = 0; i <= n; ++i) {
            float sign = (i % 2 == 0) ? 1 : -1;  // Alternating signs
            float ith_prev_sample = recentSamples[(recentSamples.size() - 1) - i * channelCount];
            float binomial = factorial[n] / (factorial[i] * factorial[n - i]);
            nth_term += sign * ith_prev_sample * binomial;
        }
        predicted += nth_term / factorial[n];
    }
    predicted = std::max(predicted, (float)(-(1 << 15)));
    predicted = std::min(predicted, (float)(1 << 15 - 1));
    return static_cast<sf::Int16>(std::round(predicted));
}

void writeHeader(BitStream &stream, uint8_t channels, uint16_t sampling_freq, uint16_t frame_size, uint32_t num_samples,
                 bool useInterleaving) {
    stream.writeBits(channels, 4);        // Up to 15 channels
    stream.writeBits(sampling_freq, 16);  // Up to 65khz sampling frequency
    stream.writeBits(frame_size, 16);     // Up to 65k samples per frame
    stream.writeBits(num_samples, 32);    // Up to 27 hours of mono audio at 44100hz
    stream.writeBits(useInterleaving, 1);
}

void readHeader(BitStream &stream, uint8_t &channels, uint16_t &sampling_freq, uint16_t &frame_size, uint32_t &num_samples,
                bool &useInterleaving) {
    channels = stream.readBits(4);
    sampling_freq = stream.readBits(16);
    frame_size = stream.readBits(16);
    num_samples = stream.readBits(32);
    useInterleaving = stream.readBits(1);
}

double get_entropy(const std::vector<int> &frameResiduals) {
    // Create a map to store the frequency of each value
    std::unordered_map<int, int> value_count;

    // Count the frequency of each value
    for (int value : frameResiduals) {
        value_count[value]++;
    }

    double total_count = frameResiduals.size();
    double entropy = 0.0;

    // Calculate the entropy
    for (const auto &[value, count] : value_count) {
        double probability = count / total_count;
        entropy -= probability * std::log2(probability);
    }

    return entropy;
}

#endif