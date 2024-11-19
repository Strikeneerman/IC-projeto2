// This program confirms that our golomb is creating the correct bits 
// by printing to the console instead of a file

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "golomb.h"

using namespace std;

bool useInterleaving = true;
int m = 5;

// Zigzag encoding (maps positive and negative integers to non-negative)
int zigzagEncode(int value) { return (value >= 0) ? (value * 2) : (-value * 2 - 1); }

// Zigzag decoding (retrieves the original signed integer)
int zigzagDecode(int value) { return (value % 2 == 0) ? (value / 2) : (-(value + 1) / 2); }

void encode(int value) {
  // Use zigzag encoding for interleaving mode or absolute value otherwise
  int encodedValue = useInterleaving ? zigzagEncode(value) : abs(value);

  uint64_t q = encodedValue / m;
  uint64_t r = encodedValue % m;
  //cout << "q: " << q << " r: " << r << endl;

  // Unary encoding of quotient q (write q ones followed by a zero)
  for (int i = 0; i < q; ++i) {
    cout << 1;
  }
  cout << 0 << ' ';

  // Binary encoding of remainder r
  int remainderBits = ceil(log2(m));
  for (int i = remainderBits - 1; i >= 0; i--) {
    cout << ((r >> i) & 1);
  }
  cout << ' ';

  // For sign and magnitude mode, write an extra bit for sign if needed
  if (!useInterleaving && value < 0) {
    cout << 1;
  } else if (!useInterleaving) {
    cout << 0;
  }
  cout << endl;
}

int main() {
    for(int i = 0; i < 20; i++){
        cout << i << ": ";
        encode(i);
    }
}