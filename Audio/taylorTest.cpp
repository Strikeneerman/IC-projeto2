#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

short int predictor_taylor(int degree, const short int* recentSamples) {
  // Start with the most recent sample
  float predicted = recentSamples[0];

  // Precompute factorial values
  std::vector<int> factorial(degree + 1, 1);
  for (int i = 1; i <= degree; ++i) {
    factorial[i] = factorial[i - 1] * i;
    // cout << "Factorial of " << i << " is " << factorial[i] << endl;
  }

  // Compute Taylor terms
  for (int n = 1; n <= degree; ++n) {
    float nth_term = 0;
    cout << "Taylor term of order " << n << ": ";

    // Approximate nth derivative using finite differences (backward Euler method)
    for (int i = 0; i <= n; ++i) {
      int sign = (i % 2 == 0) ? 1 : -1;  // Alternating signs
      nth_term += sign * (*(recentSamples - i)) * factorial[n] / (factorial[i] * factorial[n - i]);
      cout << ((i % 2 == 0) ? " + " : " - ") << factorial[n] / (factorial[i] * factorial[n - i]) << " * "
           << *(recentSamples - i);
    }
    cout << endl;

    // Add nth term to the prediction
    predicted += nth_term / factorial[n];
  }
  return static_cast<short int>(std::round(predicted));
}

int main() {
#if 0
  // Samples are from f(x) = x^2, for x = 0, 1, 2
  int taylor_degree = 2;
  short int samples[] = {0, 1, 4};
  short int predicted = predictor_taylor(taylor_degree, &samples[taylor_degree]);
  // Predicted: 8, True value would be: 9
#endif

#if 0
  // Well - behaved for a constant function
  int taylor_degree = 2;
  short int samples[] = {4, 4, 4};
  short int predicted = predictor_taylor(taylor_degree, &samples[taylor_degree]);
  // Predicted: 4, Expected: 4
#endif

#if 0
// Degree 0 returns last sample
  int taylor_degree = 0;
  short int samples[] = {0, 1, 2};
  short int predicted = predictor_taylor(taylor_degree, &samples[2]);
#endif

#if 1
  // Samples are from f(x) = x^3 - 2x^2 + 3 for x = 0, 1, 2, 3,
  int taylor_degree = 3;
  short int samples[] = {3, 2, 3, 12};
  short int predicted = predictor_taylor(taylor_degree, &samples[taylor_degree]);
  // Predicted: 26, True value would be: 35
#endif

  cout << "Predicted: " << predicted << endl;
}