#include "./SFML-2.6.1/include/SFML/Audio.hpp"

#include <complex>
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

void plotHistogram(const std::vector<double> &data, const std::string &title, int num_bins) {
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

int predictor_basic(int sample_n_1){
    return sample_n_1;
}

int main(int argc, char *argv[]) {
  std::string file_path, operation, compression_type;
  int bitrate;

  if (process_input(argc, argv, &file_path, &operation, &compression_type, &bitrate) == 1) return 1;

  // Load the WAV file
  sf::SoundBuffer buffer;
  if (!buffer.loadFromFile(file_path)) {
    std::cerr << "Failed to load WAV file: " << file_path << std::endl;
    return 1;
  }

  // Apply the predictor

  // Calculate the error (predicted-real)

  // Based on the distribution of the error, calculate the ideal parameter m (comment this for a fixed m)

  // Write header to output file with number of samples that will be coded and golomb m parameter

  // In a loop, block by block:
    // For lossy, quantize the error by n bits

    // Code with golomb

    // Calculate block bitrate, if needed, adjust n

  printf("Hello!\n");
}