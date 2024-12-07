#ifndef GOLOMB
#define GOLOMB

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "bitStream.h"

using namespace std;

class Golomb {
 private:
  int m;                 // Parameter for Golomb coding
  bool useInterleaving;  // Mode for encoding negative values

 public:
  Golomb(int m, bool useInterleaving = false) : m(m), useInterleaving(useInterleaving) {
    if (m <= 1) {
      throw invalid_argument("Parameter m must be greater than 1. Given: " + std::to_string(m));
    }
  }

  // Zigzag encoding (maps positive and negative integers to non-negative)
  int zigzagEncode(int value) { return (value >= 0) ? (value * 2) : (-value * 2 - 1); }

  // Zigzag decoding (retrieves the original signed integer)
  int zigzagDecode(int value) { return (value % 2 == 0) ? (value / 2) : (-(value + 1) / 2); }

  // Encode function that writes Golomb code to a BitStream
  // Returns number of bits written
  int encode(BitStream& stream, int value) {
    int bits_written = 0;

    // Use zigzag encoding for interleaving mode or absolute value otherwise
    int encodedValue = useInterleaving ? zigzagEncode(value) : abs(value);
    int q = encodedValue / m;
    int r = encodedValue % m;

    // Unary encoding of quotient q (write q ones followed by a zero)
    for (int i = 0; i < q; ++i) {
      stream.writeBit(1);
      bits_written++;
    }
    stream.writeBit(0);
    bits_written++;

    // Binary encoding of remainder r
    int b = ceil(log2(m));
    if (r < (1 << b) - m) {
      stream.writeBits(r, b - 1);
      bits_written += b - 1;
    } else {
      stream.writeBits(r + (1 << b) - m, b);
      bits_written += b;
    }

    // For sign and magnitude mode, write an extra bit for sign
    if (!useInterleaving && value < 0) {
      stream.writeBit(1);  // Write a sign bit (1 for negative)
      bits_written++;
    } else if (!useInterleaving) {
      stream.writeBit(0);  // Write a sign bit (0 for positive)
      bits_written++;
    }
    return bits_written;
  }

  // Decode function that reads Golomb code from a BitStream
  int decode(BitStream& stream) {
    // Decode the unary part to get quotient q
    int q = 0;
    while (stream.readBit() == 1) {  // Read unary bits until we hit 0
      ++q;
    }

    // Decode the binary part to get remainder r
    int b = ceil(log2(m));
    int r;
    if(b > 1){
        r = stream.readBits(b - 1);
        if (r >= (1 << b) - m) {
            r = ((r << 1) | (stream.readBit() ? 1 : 0)) - ((1 << b) - m);
        }
    } else {
        r = stream.readBits(b);
    }

    // Reconstruct the encoded value
    int encodedValue = q * m + r;

    // Handle interleaving or sign/magnitude decoding
    if (useInterleaving) {
      // Zigzag decode to retrieve original signed integer
      return zigzagDecode(encodedValue);
    } else {
      // Read the sign bit for sign and magnitude mode
      int signBit = stream.readBit();
      return signBit == 1 ? -encodedValue : encodedValue;
    }
  }
};

#endif
