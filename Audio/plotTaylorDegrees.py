import pandas as pd
import matplotlib.pyplot as plt

def plot_csv_values(filename):
    # Read CSV file with a single column
    data = pd.read_csv(filename, header=None, names=['value'])
    
    # Create frame numbers for x-axis
    frame_numbers = range(len(data))
    
    # Create the plot
    plt.figure(figsize=(10, 6))
    plt.plot(frame_numbers, data['value'], '-b')
    
    # Customize the plot
    plt.xlabel('Frame Number')
    plt.ylabel('Predictor Degree')
    plt.title('Predictor Degree Over Time')
    plt.grid(True)
    
    # Show the plot
    plt.show()

if __name__ == "__main__":
    # Replace 'your_file.csv' with your actual CSV filename
    plot_csv_values('./taylor_degrees.csv')