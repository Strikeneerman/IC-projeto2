#include <iostream>
#include <vector>
#include <cmath>
#include <unordered_map>


double get_entropy(const std::vector<int>& frameResiduals) {
    // Create a map to store the frequency of each value
    std::unordered_map<int, int> value_count;

    // Count the frequency of each value
    for (int value : frameResiduals) {
        value_count[value]++;
    }

    double total_count = frameResiduals.size();
    double entropy = 0.0;

    // Calculate the entropy
    for (const auto& [value, count] : value_count) {
        double probability = count / total_count;
        entropy -= probability * std::log2(probability);
    }

    return entropy;
}

int main() {
    std::vector<int> frameResiduals = {1, 2, 3, 1, 2, 3, 4, 1, 2, 5};

    double entropy = get_entropy(frameResiduals);

    std::cout << "Entropy of the signal: " << entropy << std::endl;

    return 0;
}