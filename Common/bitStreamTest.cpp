#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include "bitstream.h" // Assuming the previous BitStream class is in this header

class BinaryConverter {
private:
    // Helper function to check if character is valid (0 or 1)
    static bool isValidBinaryChar(char c) {
        return c == '0' || c == '1';
    }

public:
    // Encode text file of 0s and 1s into binary file
    static void encode(const std::string& inputFile, const std::string& outputFile) {
        std::ifstream input(inputFile);
        if (!input.is_open()) {
            throw std::runtime_error("Failed to open input file: " + inputFile);
        }

        BitStream output(outputFile, true);
        char c;
        int bitCount = 0;
        
        // Read each character and write as bit
        while (input.get(c)) {
            // Skip whitespace and newlines
            if (std::isspace(c)) continue;
            
            // Validate character
            if (!isValidBinaryChar(c)) {
                throw std::runtime_error("Invalid character in input file: " + std::string(1, c));
            }

            output.writeBit(c == '1');
            bitCount++;
        }

        // Add padding if necessary
        int paddingBits = (8 - (bitCount % 8)) % 8;
        for (int i = 0; i < paddingBits; i++) {
            output.writeBit(false);
        }

        input.close();
    }

    // Decode binary file back into text file of 0s and 1s
    static void decode(const std::string& inputFile, const std::string& outputFile) {
        BitStream input(inputFile, false);
        std::ofstream output(outputFile);
        
        if (!output.is_open()) {
            throw std::runtime_error("Failed to open output file: " + outputFile);
        }

        try {
            while (true) {
                // Read 8 bits at a time
                for (int i = 0; i < 8; i++) {
                    bool bit = input.readBit();
                    output << (bit ? '1' : '0');
                }
                output << '\n'; // Add newline after each byte for readability
            }
        } catch (const std::runtime_error& e) {
            // End of file reached - this is expected
            output.close();
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " [encode/decode] <input_file> <output_file>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string inputFile = argv[2];
    std::string outputFile = argv[3];

    try {
        if (mode == "encode") {
            BinaryConverter::encode(inputFile, outputFile);
            std::cout << "Successfully encoded file." << std::endl;
        }
        else if (mode == "decode") {
            BinaryConverter::decode(inputFile, outputFile);
            std::cout << "Successfully decoded file." << std::endl;
        }
        else {
            std::cerr << "Invalid mode. Use 'encode' or 'decode'." << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}