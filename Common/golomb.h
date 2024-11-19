#include <cmath>
#include <stdexcept>
#include <iostream>

#include "bitStream.h"  // Assumes a BitStream class is defined for bit manipulation

using namespace std;

class Golomb {
 private:
  int m;                 // Parameter for Golomb coding
  bool useInterleaving;  // Mode for encoding negative values

 public:
  // Constructor with parameter m and mode for negative values
  Golomb(int m, bool useInterleaving = false) : m(m), useInterleaving(useInterleaving) {
    if (m <= 0) {
      throw invalid_argument("Parameter m must be greater than zero.");
    }
  }

  // Zigzag encoding (maps positive and negative integers to non-negative)
  int zigzagEncode(int value) { return (value >= 0) ? (value * 2) : (-value * 2 - 1); }

  // Zigzag decoding (retrieves the original signed integer)
  int zigzagDecode(int value) { return (value % 2 == 0) ? (value / 2) : (-(value + 1) / 2); }

  // Encode function that writes Golomb code to a BitStream
  void encode(BitStream& stream, int value) {
    // Use zigzag encoding for interleaving mode or absolute value otherwise
    int encodedValue = useInterleaving ? zigzagEncode(value) : abs(value);

    int q = encodedValue / m;
    int r = encodedValue % m;
    //cout << "Encoding " << value << " q= " << q << " r= " << r << endl;

    // Unary encoding of quotient q (write q ones followed by a zero)
    for (int i = 0; i < q; ++i) {
      stream.writeBit(1);  // Write each bit of unary code
    }
    stream.writeBit(0);  // Terminate unary part

    // Binary encoding of remainder r
    int remainderBits = ceil(log2(m));
    stream.writeBits(r, remainderBits);

    // For sign and magnitude mode, write an extra bit for sign if needed
    if (!useInterleaving && value < 0) {
      stream.writeBit(1);  // Write a sign bit (1 for negative)
    } else if (!useInterleaving) {
      stream.writeBit(0);  // Write a sign bit (0 for positive)
    }
  }

  // Decode function that reads Golomb code from a BitStream
  int decode(BitStream& stream) {
    // Decode the unary part to get quotient q
    int q = 0;
    while (stream.readBit() == 1) {  // Read unary bits until we hit 0
      ++q;
    }

    // Decode the binary part to get remainder r
    int remainderBits = ceil(log2(m));
    int r = stream.readBits(remainderBits);

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
