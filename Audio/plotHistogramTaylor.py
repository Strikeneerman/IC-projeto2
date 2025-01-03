import pandas as pd
import matplotlib.pyplot as plt

def plot_csv_histogram(filename):
    # Read CSV file with a single column
    data = pd.read_csv(filename, header=None, names=['value'])
    
    # Create the histogram
    plt.figure(figsize=(10, 6))
    plt.hist(data['value'], bins=30, edgecolor='black', color='blue', alpha=0.7)
    
    # Customize the plot
    plt.xlabel('Predictor Degree')
    plt.ylabel('Frequency')
    plt.title('Distribution of Predictor Degrees')
    plt.grid(True, alpha=0.3)
    
    # Show the plot
    plt.show()

if __name__ == "__main__":
    # Replace 'your_file.csv' with your actual CSV filename
    plot_csv_histogram('taylor_degrees.csv')