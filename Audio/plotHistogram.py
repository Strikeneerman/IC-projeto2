import os
import pandas as pd
import matplotlib.pyplot as plt

# Define the folder containing the histograms
histograms_folder = './outputs/histograms/'  # Replace with the path to your folder

# Get all CSV files in the folder
csv_files = [f for f in os.listdir(histograms_folder) if f.startswith('residuals_taylor') and f.endswith('.csv')]

# Initialize variables to calculate a common x-axis range
min_bin = float('inf')
max_bin = float('-inf')

# Second pass to plot the histograms
for csv_file in csv_files:
    # Load the CSV file
    file_path = os.path.join(histograms_folder, csv_file)
    data = pd.read_csv(file_path)
    
    # Extract the columns
    bin_centers = data['Bin Center']
    frequencies = data['Frequency']
    
    # Create a new figure for each histogram
    plt.figure()
    plt.bar(bin_centers, frequencies, width=(bin_centers[1] - bin_centers[0]), edgecolor='black', align='center')
    plt.xlabel('Bin Center')
    plt.ylabel('Frequency')
    plt.title(f'Histogram: {csv_file}')
    plt.grid(True, linestyle='--', alpha=0.6)
    
    # Set the common x-axis range
    plt.ylim(0, 250000)
    plt.xlim(-5000, 5000)
    
    # Display the plot
    plt.tight_layout()
    plt.show()



"""
import pandas as pd
import matplotlib.pyplot as plt

# Load the CSV file
csv_file = './outputs/histograms/residuals_taylor_0.csv'  # Replace with the path to your CSV file
data = pd.read_csv(csv_file)

# Extract the columns
bin_centers = data['Bin Center']
frequencies = data['Frequency']

# Plot the histogram chart
plt.bar(bin_centers, frequencies, width=(bin_centers[1] - bin_centers[0]), edgecolor='black', align='center')
plt.xlabel('Bin Center')
plt.ylabel('Frequency')
plt.title('Histogram Chart')
plt.grid(True, linestyle='--', alpha=0.6)

# Display the plot
plt.tight_layout()
plt.show()
"""