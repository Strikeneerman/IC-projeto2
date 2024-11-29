#include "./SFML-2.6.2/include/SFML/Audio.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../Common/golomb.h"

// Function to print audio information
void printAudioInfo(const sf::SoundBuffer &buffer) {
  std::cout << "Audio File Information:" << std::endl;
  std::cout << "Sample Rate: " << buffer.getSampleRate() << " Hz" << std::endl;
  std::cout << "Channel Count: " << buffer.getChannelCount() << std::endl;
  std::cout << "Duration: " << buffer.getDuration().asSeconds() << " seconds" << std::endl;
  std::cout << "Sample Count: " << buffer.getSampleCount() << std::endl;
  std::cout << "Sample Size: " << sizeof(sf::Int16) * 8 << " bits" << std::endl;
}

std::vector<sf::Int16> quantizeAudio(const std::vector<sf::Int16> &samples, int bitsToReduce) {
  std::vector<sf::Int16> quantizedSamples;
  quantizedSamples.reserve(samples.size());

  for (const auto &sample : samples) {
    // Shift right to remove least significant bits
    sf::Int16 quantizedSample = sample >> bitsToReduce;
    // Shift left to restore the original scale
    quantizedSample = quantizedSample << bitsToReduce;
    quantizedSamples.push_back(quantizedSample);
  }

  return quantizedSamples;
}

void saveWav(const std::vector<sf::Int16> &samples, unsigned int sampleRate, unsigned int channelCount,
             const std::string &filename) {
  sf::SoundBuffer buffer;
  buffer.loadFromSamples(samples.data(), samples.size(), channelCount, sampleRate);

  // Ensure that the output directory exists
  std::string output_directory = "./outputs/wav_audio";
  std::filesystem::create_directory("./outputs/");
  std::filesystem::create_directory(output_directory);

  if (!buffer.saveToFile(output_directory + filename)) {
    std::cerr << "Failed to save WAV file: " << filename << std::endl;
  }
}

void saveHistogram(const std::vector<sf::Int16> &data, const std::string &title, int num_bins) {

  // Find the range of the data
  auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());
  double min_val = *min_it;
  double max_val = *max_it;

  // Create bins
  std::vector<int> bins(num_bins, 0);
  double bin_width = (max_val - min_val) / num_bins;

  // Fill the bins
  for (double value : data) {
    int bin = static_cast<int>((value - min_val) / bin_width);
    if (bin == num_bins) bin--;  // Handle edge case for maximum value
    bins[bin]++;
  }

  // Create x-axis values (bin centers)
  std::vector<double> bin_centers(num_bins);
  for (int i = 0; i < num_bins; i++) {
    bin_centers[i] = min_val + (i + 0.5) * bin_width;
  }

  // Ensure that the output directory exists
  std::string output_directory = "./outputs/histograms/";
  std::filesystem::create_directory("./outputs/");
  std::filesystem::create_directory(output_directory);

  // Save histogram data to CSV file
  std::string filename = output_directory + title + ".csv";
  std::ofstream outfile(filename);
  outfile << std::setprecision(10);  // Set precision for floating-point numbers

  // Write header
  outfile << "Bin Center,Frequency\n";

  // Write histogram data
  for (size_t i = 0; i < bins.size(); ++i) {
    outfile << bin_centers[i] << "," << bins[i] << "\n";
  }
  outfile.close();
}

int print_usage(const std::string &program_name) {
  std::cout << "Usage:\n"
            << "  " << program_name << " <file_path> encode lossy [bitrate]\n"
            << "  " << program_name << " <file_path> encode lossless\n"
            << "  " << program_name << " <file_path> decode\n";
  return 1;
}

int process_input(int argc, char *argv[], std::string *file_path, std::string *operation, std::string *compression_type,
                  int *bitrate) {
  if (argc < 3) return print_usage(argv[0]);

  *file_path = argv[1];
  *operation = argv[2];
  if (*operation == "decode") {
    if (argc != 3) return print_usage(argv[0]);
    return 0;
  }

  if (*operation != "encode") return print_usage(argv[0]);
  if (argc < 4 || argc > 5) return print_usage(argv[0]);

  *compression_type = argv[3];
  if (*compression_type == "lossless") return 0;

  if (*compression_type == "lossy") {
    if (argc != 5) return print_usage(argv[0]);
    *bitrate = std::stoi(argv[4]);
    if (*bitrate <= 0) return print_usage(argv[0]);
    return 0;
  }
  return print_usage(argv[0]);
}

sf::Int16 predictor_basic(sf::Int16 sample_n_1) { return sample_n_1; }

sf::Int16 predictor_taylor(int degree, const sf::Int16* recentSamples) {
  // Start with the most recent sample
  float predicted = recentSamples[0];

  // Precompute factorial values
  std::vector<int> factorial(degree + 1, 1);
  for (int i = 1; i <= degree; ++i) {
    factorial[i] = factorial[i - 1] * i;
  }

  // Compute Taylor terms
  for (int n = 1; n <= degree; ++n) {
    float nth_term = 0;
    // Approximate nth derivative using finite differences (backward Euler method)
    for (int i = 0; i <= n; ++i) {
      int sign = (i % 2 == 0) ? 1 : -1;  // Alternating signs
      nth_term += sign * (*(recentSamples - i)) * factorial[n] / (factorial[i] * factorial[n - i]);
    }

    // Add nth term to the prediction
    predicted += nth_term / factorial[n];
  }
  return static_cast<sf::Int16>(std::round(predicted));
}

int main(int argc, char *argv[]) {
  std::string file_path, operation, compression_type;
  int bitrate;

#if 0
  if (process_input(argc, argv, &file_path, &operation, &compression_type, &bitrate) == 1) return 1;
#else
    file_path = "./datasets/sample01.wav";
#endif
  // ============= LOAD FILE AND SPLIT CHANNELS ==================

  // Load the WAV file
  sf::SoundBuffer buffer;
  if (!buffer.loadFromFile(file_path)) {
    std::cerr << "Failed to load WAV file: " << file_path << std::endl;
    return 1;
  }

  // Read audio samples and basic info
  const sf::Int16 *samples = buffer.getSamples();
  std::size_t sampleCount = buffer.getSampleCount();
  unsigned int channelCount = buffer.getChannelCount();

  // Copy samples into a vector
  std::vector<sf::Int16> sampleVector(samples, samples + sampleCount);

  // Split the samples into their respective channels
  std::vector<std::vector<sf::Int16>> channels(channelCount);
  for (std::size_t i = 0; i < sampleCount; ++i) {
    std::size_t channelIndex = i % channelCount;
    channels[channelIndex].push_back(sampleVector[i]);
  }

  // =============== APPLY PREDICTOR AND CALCULATE RESIDUALS ==========================

  int taylor_degree = 5;
  std::vector<std::vector<sf::Int16>> residuals(channelCount);

  for (unsigned int channel = 0; channel < channelCount; ++channel) {
    const std::vector<sf::Int16> &currentChannel = channels[channel];
    std::vector<sf::Int16> &currentResiduals = residuals[channel];
    currentResiduals.reserve(currentChannel.size());

    for (std::size_t i = 0; i < currentChannel.size(); ++i) {
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


  printf("Hello!\n");
}

// Lossless:
// Read file and separate channels
// Predict and get residuals
// Get optimal m
// Write header with number of samples that will be coded, golomb m parameter, sampling frequency and channel count
// Write residuals

// Lossy:
// Read file and separate channels
// Write header without m
// Go over the file in blocks
//      Predict, get residuals, (quantize residuals), update source material
//      Get optimal m
//      Write frame header with m
//      Write frame block data
//      Calculate avg bitrate, (update quantization step)