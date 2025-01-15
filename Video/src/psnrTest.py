import numpy as np
import subprocess
import csv
from pathlib import Path
import matplotlib.pyplot as plt
import cv2

def read_y4m(filename):
    """
    Read Y4M video file and return frames as numpy array
    """
    cap = cv2.VideoCapture(filename)
    frames = []
    
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break
        # Convert to grayscale if it's not already
        if len(frame.shape) == 3:
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        frames.append(frame)
    
    cap.release()
    return np.array(frames)

def calculate_psnr(original, processed):
    """
    Calculate PSNR between original and processed frames
    """
    mse = np.mean((original - processed) ** 2)
    if mse == 0:
        return float('inf')
    max_pixel = 255.0
    psnr = 20 * np.log10(max_pixel / np.sqrt(mse))
    return psnr

def run_codec_test(original_file, quant_bits, codec_path="./video_inter"):
    """
    Run codec tests for different quantization bits and collect PSNR results
    """
    # Get the base paths
    base_dir = Path(original_file).parent.parent
    filename = Path(original_file).stem
    
    # Define paths
    output_dir = base_dir / "output"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    results = []
    
    for bits in quant_bits:
        print(f"\nTesting quantization bits: {bits}")
        
        encoded_file = output_dir / f"{filename}_enc.g7v"
        decoded_file = output_dir / f"{filename}_dec.y4m"
        
        # Run encode command
        encode_cmd = [
            codec_path,
            "-encode",
            str(original_file),
            str(encoded_file),
            "-s",
            "4",
            "-l",
            str(bits)
        ]
        print(f"Running encode command: {' '.join(encode_cmd)}")
        subprocess.run(encode_cmd)
        
        # Run decode command
        decode_cmd = [
            codec_path,
            "-decode",
            str(encoded_file),
            str(decoded_file)
        ]
        print(f"Running decode command: {' '.join(decode_cmd)}")
        subprocess.run(decode_cmd)
        
        # Read original and processed videos
        original_frames = read_y4m(str(original_file))
        processed_frames = read_y4m(str(decoded_file))
        
        # Calculate PSNR for each frame
        psnr_values = []
        for orig, proc in zip(original_frames, processed_frames):
            psnr = calculate_psnr(orig, proc)
            psnr_values.append(psnr)
        
        # Calculate average PSNR across all frames
        avg_psnr = np.mean(psnr_values)
        
        results.append({
            'quant_bits': bits,
            'avg_psnr': avg_psnr,
            'min_psnr': np.min(psnr_values),
            'max_psnr': np.max(psnr_values)
        })
        print(f"Average PSNR for {bits} bits: {avg_psnr:.2f} dB")
    
    return results

def save_results(results, output_file):
    """
    Save results to a CSV file
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['quant_bits', 'avg_psnr', 'min_psnr', 'max_psnr'])
        writer.writeheader()
        writer.writerows(results)

def plot_results(results):
    """
    Create a plot of PSNR vs Quantization Bits
    """
    bits = [r['quant_bits'] for r in results]
    avg_psnrs = [r['avg_psnr'] for r in results]
    min_psnrs = [r['min_psnr'] for r in results]
    max_psnrs = [r['max_psnr'] for r in results]
    
    plt.figure(figsize=(12, 6))
    
    # Plot average PSNR
    plt.plot(bits, avg_psnrs, 'b.-', linewidth=2, markersize=8, label='Average PSNR')
    
    # Plot min/max range
    plt.fill_between(bits, min_psnrs, max_psnrs, alpha=0.2, color='blue', label='Min-Max Range')
    
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()
    
    plt.xlabel('Quantization Bits', fontsize=12)
    plt.ylabel('PSNR (dB)', fontsize=12)
    plt.title('PSNR vs Quantization Bits', fontsize=14)
    
    # Add points to make it easier to read exact values
    for bit, psnr in zip(bits, avg_psnrs):
        plt.annotate(f'{psnr:.1f}', 
                    (bit, psnr),
                    textcoords="offset points",
                    xytext=(0,10),
                    ha='center')
    
    plt.tight_layout()
    plt.savefig('psnr_vs_quantbits.png', dpi=300, bbox_inches='tight')
    plt.show()

if __name__ == "__main__":
    # File paths
    original_file = "../videos/bowing_cif.y4m"
    
    # Define quantization bits to test
    quant_bits = range(0, 8)  # 0 to 7
    
    # Run tests
    results = run_codec_test(original_file, quant_bits)
    
    # Save results
    output_file = "codec_psnr_results.csv"
    save_results(results, output_file)
    
    # Plot results
    plot_results(results)
    
    # Print summary
    print("\nResults summary:")
    for result in results:
        print(f"Quantization Bits: {result['quant_bits']}")
        print(f"  Average PSNR: {result['avg_psnr']:.2f} dB")
        print(f"  Min PSNR: {result['min_psnr']:.2f} dB")
        print(f"  Max PSNR: {result['max_psnr']:.2f} dB")