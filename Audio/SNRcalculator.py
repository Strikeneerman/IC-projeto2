import numpy as np
from scipy.io import wavfile
import math
import subprocess
import os
import csv
from pathlib import Path
import matplotlib.pyplot as plt

def calculate_snr(original_path, processed_path):
    """
    Calculate Signal-to-Noise Ratio (SNR) between original and processed audio files.
    """
    # Read both files
    orig_rate, orig_data = wavfile.read(original_path)
    proc_rate, proc_data = wavfile.read(processed_path)
    
    # Ensure same sampling rate
    if orig_rate != proc_rate:
        raise ValueError("Sample rates do not match!")
    
    # Convert to float32 for calculation
    orig_data = orig_data.astype(np.float32)
    proc_data = proc_data.astype(np.float32)
    
    # Handle stereo files by averaging channels
    if len(orig_data.shape) > 1:
        orig_data = np.mean(orig_data, axis=1)
    if len(proc_data.shape) > 1:
        proc_data = np.mean(proc_data, axis=1)
    
    # Ensure same length
    min_length = min(len(orig_data), len(proc_data))
    orig_data = orig_data[:min_length]
    proc_data = proc_data[:min_length]
    
    # Calculate noise (difference between signals)
    noise = orig_data - proc_data
    
    # Calculate signal and noise power
    signal_power = np.sum(orig_data ** 2)
    noise_power = np.sum(noise ** 2)
    
    # Avoid division by zero
    if noise_power == 0:
        return float('inf')
    
    # Calculate SNR in dB
    snr = 10 * math.log10(signal_power / noise_power)
    
    return snr

def run_codec_test(original_file, bitrates, codec_path="./audio"):
    """
    Run codec tests for different bitrates and collect SNR results
    """
    # Get the base paths
    base_dir = Path(original_file).parent.parent
    filename = Path(original_file).stem
    
    # Define paths
    encoded_dir = base_dir / "outputs" / "encoded_audio"
    decoded_dir = base_dir / "outputs" / "wav_audio"
    
    # Ensure output directories exist
    encoded_dir.mkdir(parents=True, exist_ok=True)
    decoded_dir.mkdir(parents=True, exist_ok=True)
    
    results = []
    
    for bitrate in bitrates:
        print(f"\nTesting bitrate: {bitrate}")
        
        # Run encode command
        encode_cmd = [codec_path, f"./datasets/{filename}.wav", "encode", "lossy", str(bitrate)]
        print(f"Running encode command: {' '.join(encode_cmd)}")
        subprocess.run(encode_cmd)
        
        # Run decode command
        decode_cmd = [codec_path, f"./outputs/encoded_audio/{filename}.g7a", "decode"]
        print(f"Running decode command: {' '.join(decode_cmd)}")
        subprocess.run(decode_cmd)
        
        # Calculate SNR
        processed_file = decoded_dir / f"{filename}_decoded.wav"
        snr = calculate_snr(original_file, str(processed_file))
        
        results.append({
            'bitrate': bitrate,
            'snr': snr
        })
        print(f"SNR for bitrate {bitrate}: {snr:.2f} dB")
    
    return results

def save_results(results, output_file):
    """
    Save results to a CSV file
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['bitrate', 'snr'])
        writer.writeheader()
        writer.writerows(results)

def plot_results(results):
    """
    Create a plot of SNR vs Bitrate
    """
    bitrates = [r['bitrate'] for r in results]
    snrs = [r['snr'] for r in results]
    
    plt.figure(figsize=(10, 6))
    plt.plot(bitrates, snrs, 'b.-', linewidth=2, markersize=8)
    plt.grid(True, linestyle='--', alpha=0.7)
    
    plt.xlabel('Bitrate (bits/sample)', fontsize=12)
    plt.ylabel('SNR (dB)', fontsize=12)
    plt.title('Signal-to-Noise Ratio vs Bitrate', fontsize=14)
    
    # Add points to make it easier to read exact values
    for bitrate, snr in zip(bitrates, snrs):
        plt.annotate(f'{snr:.1f}', 
                    (bitrate, snr),
                    textcoords="offset points",
                    xytext=(0,10),
                    ha='center')
    
    plt.tight_layout()
    plt.savefig('snr_vs_bitrate.png', dpi=300, bbox_inches='tight')
    plt.show()

if __name__ == "__main__":
    # File paths
    original_file = "C:/Users/jmtia/Code projects/IC/IC-projeto2/Audio/datasets/sample05.wav"
    
    # Define bitrates to test
    bitrates = list(range(100, 200, 100)) + \
            list(range(200, 300, 10)) + \
            list(range(300, 400, 20)) + \
            list(range(400, 600, 100)) + \
            list(range(600, 1600, 200))
    
    # Run tests
    results = run_codec_test(original_file, bitrates)
    
    # Save results
    output_file = "codec_snr_results.csv"
    save_results(results, output_file)
    
    # Plot results
    plot_results(results)
    
    # Print summary
    print("\nResults summary:")
    for result in results:
        print(f"Bitrate: {result['bitrate']}, SNR: {result['snr']:.2f} dB")