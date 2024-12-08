#include "./SFML-2.6.2/include/SFML/Audio.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "./decoder.h"
#include "./encoder.h"

int print_usage(const std::string &program_name) {
  std::cout << "Usage:\n"
            << "  " << program_name << " <file_path> encode lossy [bitrate]\n"
            << "  " << program_name << " <file_path> encode lossless\n"
            << "  " << program_name << " <file_path> decode\n";
  return 1;
}

int process_input(int argc, char *argv[], std::string &file_path, std::string &operation, std::string &compression_type, int &bitrate) {
  if (argc < 3) return print_usage(argv[0]);
  file_path = argv[1];
  operation = argv[2];
  if (operation == "decode") {
    if (argc != 3) return print_usage(argv[0]);
    return 0;
  }
  if (operation != "encode") return print_usage(argv[0]);
  if (argc < 4 || argc > 5) return print_usage(argv[0]);
  compression_type = argv[3];
  if (compression_type == "lossless") return 0;
  if (compression_type == "lossy") {
    if (argc != 5) return print_usage(argv[0]);
    bitrate = std::stoi(argv[4]);
    if (bitrate <= 0) return print_usage(argv[0]);
    return 0;
  }
  return print_usage(argv[0]);
}

// Main
int main(int argc, char *argv[]) {
  std::string file_path, operation, compression_type;
  int bitrate = 0;

#if 1
  if (process_input(argc, argv, file_path, operation, compression_type, bitrate) == 1) return 1;
#else
  file_path = "./datasets/sample01.wav";
  operation = "encode";
  compression_type = "lossless";
  bitrate = 0;
#endif

  if (operation == "encode") {
    return encode(file_path, compression_type, bitrate);
  } else if (operation == "decode") {
    return decode(file_path);
  }
}