#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "golomb.h"

using namespace std;

// Function to read integers from a text file
vector<int> readIntegersFromFile(const string& filename) {
  vector<int> integers;
  ifstream inputFile(filename);

  if (!inputFile.is_open()) {
    throw runtime_error("Failed to open input file: " + filename);
  }

  int number;
  while (inputFile >> number) {
    integers.push_back(number);
  }

  inputFile.close();
  return integers;
}

// Function to encode integers and write them to a binary file
void encodeIntegersToFile(const vector<int>& integers, const string& binaryFilename, Golomb& golomb) {
  BitStream bitStream(binaryFilename, true);  // Open in write mode

  for (int value : integers) {
    golomb.encode(bitStream, value);
  }
}

// Function to decode integers from a binary file
vector<int> decodeIntegersFromFile(const string& binaryFilename, Golomb& golomb) {
  BitStream bitStream(binaryFilename, false);  // Open in read mode
  vector<int> decodedIntegers;

  try {
    while (true) {
      int decodedValue = golomb.decode(bitStream);
      decodedIntegers.push_back(decodedValue);
    }
  } catch (const runtime_error& e) {
    // End of file or decoding error, stop reading
  }

  return decodedIntegers;
}

// Function to compare original and decoded integers
void compareResults(const vector<int>& original, const vector<int>& decoded) {
  bool isAccurate = true;

  if (original.size() != decoded.size()) {
    cout << "Mismatch in number of integers: "
         << "original = " << original.size() << ", decoded = " << decoded.size() << "\n";
    isAccurate = false;
  }

  for (size_t i = 0; i < original.size(); ++i) {
    if (i >= decoded.size() || original[i] != decoded[i]) {
      cout << "Mismatch at index " << i << ": "
           << "original = " << original[i] << ", decoded = " << decoded[i] << "\n";
      isAccurate = false;
    }
  }

  if (isAccurate) {
    cout << "All integers were encoded and decoded accurately.\n";
  } else {
    cout << "There were mismatches between original and decoded values.\n";
  }
}

int main() {
  try {
    // Parameters
    const string inputFilename = "test.txt";
    const string binaryFilename = "encoded.bin";
    const int m = 7;                    // Example Golomb parameter; adjust as needed
    const bool useInterleaving = false;  // Use interleaving mode for negative numbers

    // Step 1: Read integers from text file
    vector<int> originalIntegers = readIntegersFromFile(inputFilename);

    cout << "Original ints:" << endl;
    for (int value : originalIntegers) {
      cout << value << endl;
    }

    // Step 2: Initialize Golomb encoder and encode integers
    Golomb golomb(m, useInterleaving);
    encodeIntegersToFile(originalIntegers, binaryFilename, golomb);

    // Step 3: Decode integers from binary file
    vector<int> decodedIntegers = decodeIntegersFromFile(binaryFilename, golomb);

    cout << "Decoded ints:" << endl;
    for (int value : decodedIntegers) {
      cout << value << endl;
    }

    // Step 4: Compare and print results
    compareResults(originalIntegers, decodedIntegers);

  } catch (const exception& e) {
    cerr << "Error: " << e.what() << "\n";
  }

  return 0;
}
