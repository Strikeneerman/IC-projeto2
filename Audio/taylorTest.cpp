#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

// There are issues with the predictor nor degree higuer than 0
short int predictor_taylor(int degree, int channelCount, const std::vector<short int> &recentSamples) {
    if (recentSamples.size() / channelCount == 0) { 
        return 0;                                                   // No recent samples, return 0
    }
    if (degree >= recentSamples.size() / channelCount) {
        return recentSamples[recentSamples.size() - channelCount];  // Not enough samples for a prediction, return last sample
    }

    // Precompute factorial values
    std::vector<int> factorial(degree + 1);
    factorial[0] = 1;
    for (int i = 1; i <= degree; ++i) {
        factorial[i] = factorial[i - 1] * i;
    }

    // Compute Taylor terms approximating derivatives using finite differences (backward Euler method)
    float predicted = recentSamples[recentSamples.size() - channelCount];
    for (int n = 1; n <= degree; ++n) {
        cout << endl << "Taylor term of order " << n << ": ";
        float nth_term = 0;
        for (int i = 0; i <= n; ++i) {
            float sign = (i % 2 == 0) ? 1 : -1;  // Alternating signs
            float ith_prev_sample = recentSamples[(recentSamples.size() - 1) - i * channelCount];
            float binomial = factorial[n] / (factorial[i] * factorial[n - i]);
            nth_term += sign * ith_prev_sample * binomial;
            cout << (sign == 1 ? " +" : " -") << binomial << "*" << ith_prev_sample;
        }
        predicted += nth_term / factorial[n];
    }
    predicted = std::max(predicted, (float)(-(1 << 15)));
    predicted = std::min(predicted, (float)(1 << 15 - 1));
    return static_cast<short int>(std::round(predicted));
}

int main() {
    // Samples are from f(x) = x^2, for x = 0, 1, 2
    int taylor_degree = 3;
    std::vector<short int> samples;
    for (int i = 0; i < 4; i++) {
        samples.push_back(i * i);
        cout << samples[i] << " ";
    }
    short int predicted = predictor_taylor(taylor_degree, 1, samples);

    cout << endl;
    cout << "Predicted: " << predicted << endl;
}

/*
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

#if 0
  // Samples are from f(x) = x^3 - 2x^2 + 3 for x = 0, 1, 2, 3,
  int taylor_degree = 3;
  short int samples[] = {3, 2, 3, 12};
  short int predicted = predictor_taylor(taylor_degree, &samples[taylor_degree]);
  // Predicted: 26, True value would be: 35
#endif
*/

/*
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
}*/