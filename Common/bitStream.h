#ifndef BITSTREAM
#define BITSTREAM

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>

class BitStream {
private:
    std::fstream file;
    unsigned char buffer;  // 8-bit buffer for reading/writing
    int bufferPos;        // Current position in buffer (0-7)
    bool isWriteMode;     // Track if we're in write mode

    // Helper method to flush buffer to file
    void flushBuffer() {
        if (isWriteMode && bufferPos > 0) {
            // Pad the remaining bits with zeros
            buffer <<= (8 - bufferPos);
            file.write(reinterpret_cast<char*>(&buffer), 1);
            buffer = 0;
            bufferPos = 0;
        }
    }

public:
    BitStream(const std::string& filename, bool write = true) :
        buffer(0), bufferPos(0), isWriteMode(write) {
        if (write) {
            file.open(filename, std::ios::out | std::ios::binary);
        } else {
            file.open(filename, std::ios::in | std::ios::binary);
        }

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
    }

    ~BitStream() {
        if (isWriteMode) {
            flushBuffer();
        }
        file.close();
    }

    // Write a single bit to the file
    void writeBit(bool bit) {
        if (!isWriteMode) {
            throw std::runtime_error("Stream not in write mode");
        }

        buffer = (buffer) | ((bit ? 1 : 0) << (7 - bufferPos));
        bufferPos++;

        if (bufferPos == 8) {
            file.write(reinterpret_cast<char*>(&buffer), 1);
            buffer = 0;
            bufferPos = 0;
        }
    }

    // New method to check if the end of file has been reached
    bool eof() {
        if (bufferPos < 8) {  // Still bits left in the buffer
            return false;
        }
        return file.eof();  // Check if end of file flag is set
    }

    // Read a single bit from the file
    bool readBit() {
        if (isWriteMode) {
            throw std::runtime_error("Stream not in read mode");
        }

        if (bufferPos == 0 || bufferPos == 8) {
            char nextByte;
            if (!file.read(&nextByte, 1)) {
                if (file.eof()) {
                    return false;  // Return false if end of file is reached
                } else {
                    throw std::runtime_error("Error reading from file");
                }
            }
            buffer = static_cast<unsigned char>(nextByte);
            bufferPos = 0;
        }

        bool bit = (buffer >> (7 - bufferPos)) & 1;
        bufferPos++;
        return bit;
    }

    // Write N bits of an integer to the file (0 < N <= 64)
    void writeBits(uint64_t value, int N) {
        if (N <= 0 || N > 64) {
            throw std::invalid_argument("N must be between 1 and 64. Given: " + std::to_string(N));
        }

        for (int i = N - 1; i >= 0; i--) {
            writeBit((value >> i) & 1);
        }
    }

    // Read N bits from the file into an integer (0 < N <= 64)
    uint64_t readBits(int N) {
        if (N <= 0 || N > 64) {
            throw std::invalid_argument("N must be between 1 and 64. Given: " + std::to_string(N));
        }

        uint64_t result = 0;
        for (int i = 0; i < N; i++) {
            result = (result << 1) | (readBit() ? 1 : 0);
        }
        return result;
    }

    // Write a string as bits
    void writeString(const std::string& str) {
        for (char c : str) {
            writeBits(static_cast<uint64_t>(c), 8);
        }
    }

    // Read a string of specified length
    std::string readString(size_t length) {
        std::string result;
        for (size_t i = 0; i < length; i++) {
            char c = static_cast<char>(readBits(8));
            result += c;
        }
        return result;
    }
};

#endif