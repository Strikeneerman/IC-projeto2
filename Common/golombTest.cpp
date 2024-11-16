#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "golomb.h"

// Function to read integers from a text file
std::vector<int> readIntegersFromFile(const std::string& filename) {
    std::vector<int> integers;
    std::ifstream inputFile(filename);

    if (!inputFile.is_open()) {
        throw std::runtime_error("Failed to open input file: " + filename);
    }

    int number;
    while (inputFile >> number) {
        integers.push_back(number);
    }

    inputFile.close();
    return integers;
}

// Function to encode integers and write them to a binary file
void encodeIntegersToFile(const std::vector<int>& integers, const std::string& binaryFilename, Golomb& golomb) {
    BitStream bitStream(binaryFilename, true);  // Open in write mode

    for (int value : integers) {
        golomb.encode(bitStream, value);
    }
}

// Function to decode integers from a binary file
std::vector<int> decodeIntegersFromFile(const std::string& binaryFilename, Golomb& golomb) {
    BitStream bitStream(binaryFilename, false);  // Open in read mode
    std::vector<int> decodedIntegers;

    try {
        while (true) {
            int decodedValue = golomb.decode(bitStream);
            decodedIntegers.push_back(decodedValue);
        }
    } catch (const std::runtime_error& e) {
        // End of file or decoding error, stop reading
    }

    return decodedIntegers;
}

// Function to compare original and decoded integers
void compareResults(const std::vector<int>& original, const std::vector<int>& decoded) {
    bool isAccurate = true;

    if (original.size() != decoded.size()) {
        std::cout << "Mismatch in number of integers: "
                  << "original = " << original.size() << ", decoded = " << decoded.size() << "\n";
        isAccurate = false;
    }

    for (size_t i = 0; i < original.size(); ++i) {
        if (i >= decoded.size() || original[i] != decoded[i]) {
            std::cout << "Mismatch at index " << i << ": "
                      << "original = " << original[i] << ", decoded = " << decoded[i] << "\n";
            isAccurate = false;
        }
    }

    if (isAccurate) {
        std::cout << "All integers were encoded and decoded accurately.\n";
    } else {
        std::cout << "There were mismatches between original and decoded values.\n";
    }
}

int main() {
    try {
        // Parameters
        const std::string inputFilename = "test.txt";
        const std::string binaryFilename = "encoded.bin";
        const int m = 5;  // Example Golomb parameter; adjust as needed
        const bool useInterleaving = true;  // Use interleaving mode for negative numbers

        // Step 1: Read integers from text file
        std::vector<int> originalIntegers = readIntegersFromFile(inputFilename);

        // Step 2: Initialize Golomb encoder and encode integers
        Golomb golomb(m, useInterleaving);
        encodeIntegersToFile(originalIntegers, binaryFilename, golomb);

        // Step 3: Decode integers from binary file
        std::vector<int> decodedIntegers = decodeIntegersFromFile(binaryFilename, golomb);

        // Step 4: Compare and print results
        compareResults(originalIntegers, decodedIntegers);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
