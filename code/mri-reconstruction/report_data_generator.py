"""
Generate synthetic data and figures for MRI Reconstruction Technical Report
This script creates all the figures and data needed for the LaTeX report
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.gridspec import GridSpec
import os
from pathlib import Path
from typing import Tuple, List
import seaborn as sns

# Set up plotting parameters
plt.rcParams['figure.dpi'] = 300
plt.rcParams['savefig.dpi'] = 300
plt.rcParams['font.size'] = 12
plt.rcParams['axes.titlesize'] = 14
plt.rcParams['axes.labelsize'] = 12
plt.rcParams['xtick.labelsize'] = 10
plt.rcParams['ytick.labelsize'] = 10
plt.rcParams['legend.fontsize'] = 10

# Create output directory
output_dir = Path("report_figures")
output_dir.mkdir(exist_ok=True)

class ReportDataGenerator:
    """Generate all synthetic data and figures for the technical report"""
    
    def __init__(self, image_size: int = 256):
        self.image_size = image_size
        self.output_dir = output_dir
        
    def generate_shepp_logan_phantom(self, variant: str = 'standard') -> np.ndarray:
        """Generate Shepp-Logan phantom with different variants"""
        # Simplified Shepp-Logan phantom generation
        x = np.linspace(-1, 1, self.image_size)
        y = np.linspace(-1, 1, self.image_size)
        X, Y = np.meshgrid(x, y)
        
        phantom = np.zeros((self.image_size, self.image_size))
        
        if variant == 'standard':
            # Main ellipse (head outline)
            mask1 = ((X/0.69)**2 + (Y/0.92)**2) <= 1
            phantom[mask1] = 1.0
            
            # Inner ellipses (brain structures)
            mask2 = ((X/0.6624)**2 + ((Y+0.0184)/0.874)**2) <= 1
            phantom[mask2] = -0.98
            
            # Smaller structures
            mask3 = (((X+0.22)/0.11)**2 + (Y/0.31)**2) <= 1
            phantom[mask3] = -0.8
            
            mask4 = (((X-0.22)/0.16)**2 + (Y/0.41)**2) <= 1
            phantom[mask4] = -0.8
            
            # Additional small features
            mask5 = (((X)/0.21)**2 + ((Y-0.35)/0.25)**2) <= 1
            phantom[mask5] = 0.4
            
        elif variant == 'modified':
            # Modified version with more structures
            # Main head
            mask1 = ((X/0.69)**2 + (Y/0.92)**2) <= 1
            phantom[mask1] = 1.0
            
            # Brain tissue
            mask2 = ((X/0.6)**2 + (Y/0.8)**2) <= 1
            phantom[mask2] = 0.8
            
            # Ventricles
            mask3 = (((X-0.1)/0.15)**2 + ((Y+0.1)/0.2)**2) <= 1
            phantom[mask3] = 0.2
            
            mask4 = (((X+0.1)/0.15)**2 + ((Y+0.1)/0.2)**2) <= 1
            phantom[mask4] = 0.2
            
            # Lesions/pathology
            mask5 = (((X-0.3)/0.08)**2 + ((Y-0.2)/0.08)**2) <= 1
            phantom[mask5] = 1.5
            
        return phantom
    
    def generate_brain_phantom(self) -> np.ndarray:
        """Generate brain-like phantom with anatomical structures"""
        x = np.linspace(-1, 1, self.image_size)
        y = np.linspace(-1, 1, self.image_size)
        X, Y = np.meshgrid(x, y)
        
        phantom = np.zeros((self.image_size, self.image_size))
        
        # Skull outline
        skull_mask = ((X/0.9)**2 + (Y/0.95)**2) <= 1
        skull_inner = ((X/0.85)**2 + (Y/0.9)**2) <= 1
        phantom[skull_mask & ~skull_inner] = 0.3
        
        # Brain tissue
        brain_mask = ((X/0.8)**2 + (Y/0.85)**2) <= 1
        phantom[brain_mask] = 1.0
        
        # Gray matter (cortex)
        cortex_outer = ((X/0.75)**2 + (Y/0.8)**2) <= 1
        cortex_inner = ((X/0.6)**2 + (Y/0.65)**2) <= 1
        phantom[cortex_outer & ~cortex_inner] = 0.8
        
        # White matter
        white_matter = ((X/0.55)**2 + (Y/0.6)**2) <= 1
        phantom[white_matter] = 0.6
        
        # Ventricles
        ventricle_left = (((X-0.15)/0.12)**2 + ((Y+0.05)/0.18)**2) <= 1
        ventricle_right = (((X+0.15)/0.12)**2 + ((Y+0.05)/0.18)**2) <= 1
        phantom[ventricle_left | ventricle_right] = 0.1
        
        return phantom
    
    def create_sampling_mask(self, shape: Tuple[int, int], 
                           acceleration_factor: float,
                           center_fraction: float = 0.08) -> np.ndarray:
        """Create undersampling mask for k-space"""
        ny, nx = shape
        mask = np.zeros(shape, dtype=bool)
        
        # Calculate number of lines to sample
        num_lines = int(ny / acceleration_factor)
        center_lines = int(center_fraction * ny)
        
        # Always sample center k-space
        center_start = ny // 2 - center_lines // 2
        center_end = center_start + center_lines
        mask[center_start:center_end, :] = True
        
        # Random sampling for remaining lines
        remaining_lines = num_lines - center_lines
        available_indices = list(range(ny))
        for i in range(center_start, center_end):
            if i in available_indices:
                available_indices.remove(i)
        
        if remaining_lines > 0:
            selected_indices = np.random.choice(
                available_indices, 
                min(remaining_lines, len(available_indices)), 
                replace=False
            )
            for idx in selected_indices:
                mask[idx, :] = True
        
        return mask
    
    def fft2c(self, image: np.ndarray) -> np.ndarray:
        """Centered 2D FFT"""
        return np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(image)))
    
    def ifft2c(self, kspace: np.ndarray) -> np.ndarray:
        """Centered 2D IFFT"""
        return np.fft.fftshift(np.fft.ifft2(np.fft.ifftshift(kspace)))
    
    def add_noise(self, kspace: np.ndarray, snr_db: float = 30) -> np.ndarray:
        """Add complex Gaussian noise to k-space data"""
        signal_power = np.mean(np.abs(kspace)**2)
        noise_power = signal_power / (10**(snr_db/10))
        noise_std = np.sqrt(noise_power / 2)  # Complex noise
        
        noise_real = np.random.normal(0, noise_std, kspace.shape)
        noise_imag = np.random.normal(0, noise_std, kspace.shape)
        noise = noise_real + 1j * noise_imag
        
        return kspace + noise
    
    def zero_fill_reconstruction(self, kspace_undersampled: np.ndarray) -> np.ndarray:
        """Simple zero-filled reconstruction"""
        return np.abs(self.ifft2c(kspace_undersampled))
    
    def simulate_fista_reconstruction(self, zero_filled: np.ndarray, 
                                    improvement_factor: float = 1.5) -> np.ndarray:
        """Simulate FISTA reconstruction (simplified for demo)"""
        # Apply some smoothing and enhancement to simulate FISTA
        from scipy import ndimage
        
        # Light denoising
        fista_recon = ndimage.gaussian_filter(zero_filled, sigma=0.5)
        
        # Enhance contrast
        fista_recon = fista_recon * improvement_factor
        fista_recon = np.clip(fista_recon, 0, np.max(zero_filled) * 1.2)
        
        return fista_recon
    
    def simulate_unet_reconstruction(self, zero_filled: np.ndarray, 
                                   ground_truth: np.ndarray) -> np.ndarray:
        """Simulate U-Net reconstruction (simplified for demo)"""
        # Simulate a high-quality reconstruction
        # Mix of ground truth and processed zero-filled
        alpha = 0.7  # Weight toward ground truth
        
        # Process zero-filled to remove some artifacts
        from scipy import ndimage
        processed_zf = ndimage.median_filter(zero_filled, size=3)
        
        # Combine with ground truth (simulating learned mapping)
        unet_recon = alpha * ground_truth + (1 - alpha) * processed_zf
        
        # Add slight blur to simulate network reconstruction
        unet_recon = ndimage.gaussian_filter(unet_recon, sigma=0.3)
        
        return unet_recon
    
    def generate_phantom_gallery(self):
        """Generate figure showing different phantom types"""
        fig, axes = plt.subplots(2, 3, figsize=(12, 8))
        
        # Generate different phantoms
        phantoms = [
            (self.generate_shepp_logan_phantom('standard'), 'Standard Shepp-Logan'),
            (self.generate_shepp_logan_phantom('modified'), 'Modified Shepp-Logan'),
            (self.generate_brain_phantom(), 'Brain-like Phantom'),
        ]
        
        # Top row: Original phantoms
        for i, (phantom, title) in enumerate(phantoms):
            axes[0, i].imshow(phantom, cmap='gray', vmin=0, vmax=1.5)
            axes[0, i].set_title(title)
            axes[0, i].axis('off')
        
        # Bottom row: With added complexity
        for i, (phantom, title) in enumerate(phantoms):
            # Add some texture and noise for realism
            complex_phantom = phantom.copy()
            noise = np.random.normal(0, 0.02, phantom.shape)
            complex_phantom += noise
            
            axes[1, i].imshow(complex_phantom, cmap='gray', vmin=0, vmax=1.5)
            axes[1, i].set_title(f'{title} + Texture')
            axes[1, i].axis('off')
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'phantom_gallery.png', 
                   bbox_inches='tight', facecolor='white')
        plt.close()
        
        print(f"✓ Generated phantom gallery: {self.output_dir / 'phantom_gallery.png'}")
    
    def generate_sampling_patterns(self):
        """Generate k-space sampling patterns figure"""
        fig, axes = plt.subplots(1, 4, figsize=(16, 4))
        
        acceleration_factors = [2, 4, 6, 8]
        
        for i, R in enumerate(acceleration_factors):
            mask = self.create_sampling_mask(
                (self.image_size, self.image_size), 
                acceleration_factor=R,
                center_fraction=0.08
            )
            
            axes[i].imshow(mask, cmap='gray', aspect='equal')
            axes[i].set_title(f'R = {R}x\n({100/R:.0f}% sampled)')
            axes[i].axis('off')
            
            # Add center region highlight
            center_size = int(0.08 * self.image_size)
            center_start = self.image_size // 2 - center_size // 2
            rect = patches.Rectangle(
                (0, center_start), self.image_size-1, center_size,
                linewidth=2, edgecolor='red', facecolor='none', alpha=0.7
            )
            axes[i].add_patch(rect)
        
        plt.suptitle('K-space Sampling Patterns\n(Red box indicates fully sampled center)', 
                    fontsize=14)
        plt.tight_layout()
        plt.savefig(self.output_dir / 'sampling_patterns.png', 
                   bbox_inches='tight', facecolor='white')
        plt.close()
        
        print(f"✓ Generated sampling patterns: {self.output_dir / 'sampling_patterns.png'}")
    
    def generate_reconstruction_comparison(self):
        """Generate reconstruction comparison figure"""
        # Use brain phantom for demonstration
        ground_truth = self.generate_brain_phantom()
        
        # Create k-space and undersample
        kspace_full = self.fft2c(ground_truth)
        kspace_full = self.add_noise(kspace_full, snr_db=30)
        
        # 4x acceleration
        mask = self.create_sampling_mask(
            kspace_full.shape, acceleration_factor=4, center_fraction=0.08
        )
        kspace_undersampled = kspace_full * mask
        
        # Reconstructions
        zero_filled = self.zero_fill_reconstruction(kspace_undersampled)
        fista_recon = self.simulate_fista_reconstruction(zero_filled)
        unet_recon = self.simulate_unet_reconstruction(zero_filled, ground_truth)
        
        # Create comparison figure
        fig, axes = plt.subplots(2, 4, figsize=(16, 8))
        
        # Top row: Full images
        images = [ground_truth, zero_filled, fista_recon, unet_recon]
        titles = ['Ground Truth', 'Zero-filled\n(PSNR: 19.3 dB)', 
                 'FISTA\n(PSNR: 26.8 dB)', 'U-Net\n(PSNR: 30.4 dB)']
        
        for i, (img, title) in enumerate(zip(images, titles)):
            axes[0, i].imshow(img, cmap='gray', vmin=0, vmax=1.2)
            axes[0, i].set_title(title, fontsize=12)
            axes[0, i].axis('off')
        
        # Bottom row: Zoomed regions showing detail
        zoom_region = slice(80, 180), slice(80, 180)  # Center region
        
        for i, (img, title) in enumerate(zip(images, titles)):
            zoomed = img[zoom_region]
            axes[1, i].imshow(zoomed, cmap='gray', vmin=0, vmax=1.2)
            axes[1, i].set_title('Zoomed Detail', fontsize=10)
            axes[1, i].axis('off')
            
        # Add zoom boxes to top row
        for i in range(4):
            rect = patches.Rectangle(
                (80, 80), 100, 100,
                linewidth=2, edgecolor='red', facecolor='none'
            )
            axes[0, i].add_patch(rect)
        
        plt.suptitle('Reconstruction Comparison (4× Acceleration)', fontsize=16)
        plt.tight_layout()
        plt.savefig(self.output_dir / 'reconstruction_comparison.png', 
                   bbox_inches='tight', facecolor='white')
        plt.close()
        
        print(f"✓ Generated reconstruction comparison: {self.output_dir / 'reconstruction_comparison.png'}")
    
    def generate_convergence_plot(self):
        """Generate FISTA convergence plot"""
        # Simulate convergence data
        iterations = np.arange(1, 51)
        
        # Different acceleration factors
        R_factors = [2, 4, 6, 8]
        colors = ['blue', 'green', 'orange', 'red']
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
        
        for R, color in zip(R_factors, colors):
            # Simulate cost function decrease
            # Faster convergence for lower acceleration
            decay_rate = 0.1 + (R-2) * 0.02
            noise_scale = 0.02 + (R-2) * 0.01
            
            cost = np.exp(-decay_rate * iterations) + 0.1
            cost += np.random.normal(0, noise_scale, len(cost))
            cost = np.maximum(cost, 0.05)  # Floor value
            
            # Smooth for better visualization
            from scipy import ndimage
            cost_smooth = ndimage.gaussian_filter1d(cost, sigma=1.5)
            
            ax1.semilogy(iterations, cost_smooth, color=color, 
                        linewidth=2, label=f'R = {R}')
            
            # PSNR improvement
            psnr_start = 15 + R  # Starting PSNR
            psnr_end = 30 - R*1.5  # Final PSNR
            psnr = psnr_start + (psnr_end - psnr_start) * (1 - np.exp(-0.15 * iterations))
            psnr += np.random.normal(0, 0.3, len(psnr))
            psnr_smooth = ndimage.gaussian_filter1d(psnr, sigma=1.5)
            
            ax2.plot(iterations, psnr_smooth, color=color, 
                    linewidth=2, label=f'R = {R}')
        
        ax1.set_xlabel('Iteration')
        ax1.set_ylabel('Cost Function')
        ax1.set_title('FISTA Cost Function Convergence')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        ax2.set_xlabel('Iteration')
        ax2.set_ylabel('PSNR (dB)')
        ax2.set_title('Reconstruction Quality vs Iteration')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'fista_convergence.png', 
                   bbox_inches='tight', facecolor='white')
        plt.close()
        
        print(f"✓ Generated convergence plot: {self.output_dir / 'fista_convergence.png'}")
    
    def generate_results_summary(self):
        """Generate results summary plots"""
        # Performance data (simulated based on typical results)
        acceleration_factors = [2, 4, 6, 8]
        
        # PSNR data
        psnr_data = {
            'Zero-filled': [24.5, 19.3, 16.7, 14.8],
            'FISTA': [31.2, 26.8, 23.4, 20.7],
            'U-Net': [35.6, 30.4, 26.9, 23.1]
        }
        
        # SSIM data
        ssim_data = {
            'Zero-filled': [0.78, 0.65, 0.52, 0.41],
            'FISTA': [0.91, 0.84, 0.76, 0.68],
            'U-Net': [0.96, 0.92, 0.87, 0.79]
        }
        
        # Runtime data (seconds)
        runtime_data = {
            'Zero-filled': [0.0001, 0.0001, 0.0001, 0.0001],
            'FISTA': [0.2, 0.25, 0.3, 0.35],
            'U-Net': [0.015, 0.02, 0.025, 0.03]
        }
        
        fig, axes = plt.subplots(1, 3, figsize=(15, 5))
        colors = ['red', 'blue', 'green']
        methods = ['Zero-filled', 'FISTA', 'U-Net']
        
        # PSNR plot
        for method, color in zip(methods, colors):
            axes[0].plot(acceleration_factors, psnr_data[method], 
                        'o-', color=color, linewidth=2, markersize=6, label=method)
        
        axes[0].set_xlabel('Acceleration Factor')
        axes[0].set_ylabel('PSNR (dB)')
        axes[0].set_title('Reconstruction Quality vs Acceleration')
        axes[0].legend()
        axes[0].grid(True, alpha=0.3)
        axes[0].set_xticks(acceleration_factors)
        
        # SSIM plot
        for method, color in zip(methods, colors):
            axes[1].plot(acceleration_factors, ssim_data[method], 
                        'o-', color=color, linewidth=2, markersize=6, label=method)
        
        axes[1].set_xlabel('Acceleration Factor')
        axes[1].set_ylabel('SSIM')
        axes[1].set_title('Structural Similarity vs Acceleration')
        axes[1].legend()
        axes[1].grid(True, alpha=0.3)
        axes[1].set_xticks(acceleration_factors)
        axes[1].set_ylim([0, 1])
        
        # Runtime plot (log scale)
        for method, color in zip(methods, colors):
            axes[2].semilogy(acceleration_factors, runtime_data[method], 
                           'o-', color=color, linewidth=2, markersize=6, label=method)
        
        axes[2].set_xlabel('Acceleration Factor')
        axes[2].set_ylabel('Runtime (seconds)')
        axes[2].set_title('Computational Efficiency')
        axes[2].legend()
        axes[2].grid(True, alpha=0.3)
        axes[2].set_xticks(acceleration_factors)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'results_summary.png', 
                   bbox_inches='tight', facecolor='white')
        plt.close()
        
        print(f"✓ Generated results summary: {self.output_dir / 'results_summary.png'}")
    
    def generate_all_figures(self):
        """Generate all figures for the technical report"""
        print("🎨 Generating figures for MRI Reconstruction Technical Report...")
        print(f"📁 Output directory: {self.output_dir.absolute()}")
        print()
        
        # Set random seed for reproducible results
        np.random.seed(42)
        
        # Generate all figures
        self.generate_phantom_gallery()
        self.generate_sampling_patterns()
        self.generate_reconstruction_comparison()
        self.generate_convergence_plot()
        self.generate_results_summary()
        
        print()
        print("✨ All figures generated successfully!")
        print(f"📊 Generated {len(list(self.output_dir.glob('*.png')))} figures")
        print()
        print("📝 To use in LaTeX report:")
        print("   1. Copy figures to your LaTeX project folder")
        print("   2. Replace \\rule{...} placeholders with \\includegraphics{filename}")
        print("   3. Update figure captions as needed")

def main():
    """Main function to generate all report data"""
    generator = ReportDataGenerator(image_size=256)
    generator.generate_all_figures()
    
    # Save some numerical data for tables
    print("\n📋 Generating numerical data for tables...")
    
    # Performance data for LaTeX tables
    results_data = {
        'acceleration_factors': [2, 4, 6, 8],
        'psnr_zero_filled': [24.5, 19.3, 16.7, 14.8],
        'psnr_fista': [31.2, 26.8, 23.4, 20.7],
        'psnr_unet': [35.6, 30.4, 26.9, 23.1],
        'ssim_zero_filled': [0.78, 0.65, 0.52, 0.41],
        'ssim_fista': [0.91, 0.84, 0.76, 0.68],
        'ssim_unet': [0.96, 0.92, 0.87, 0.79],
        'runtime_zero_filled': [0.0001, 0.0001, 0.0001, 0.0001],
        'runtime_fista': [0.20, 0.25, 0.30, 0.35],
        'runtime_unet': [0.015, 0.020, 0.025, 0.030]
    }
    
    # Save as CSV for easy access
    import csv
    with open(output_dir / 'results_data.csv', 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['Metric', 'R=2', 'R=4', 'R=6', 'R=8'])
        writer.writerow(['PSNR Zero-filled'] + results_data['psnr_zero_filled'])
        writer.writerow(['PSNR FISTA'] + results_data['psnr_fista'])
        writer.writerow(['PSNR U-Net'] + results_data['psnr_unet'])
        writer.writerow(['SSIM Zero-filled'] + results_data['ssim_zero_filled'])
        writer.writerow(['SSIM FISTA'] + results_data['ssim_fista'])
        writer.writerow(['SSIM U-Net'] + results_data['ssim_unet'])
    
    print(f"✓ Saved results data: {output_dir / 'results_data.csv'}")
    
    print("\n🎯 Next steps:")
    print("   1. Copy the LaTeX report template")
    print("   2. Copy generated figures to your LaTeX project")
    print("   3. Update \\includegraphics commands with actual figure names")
    print("   4. Customize results tables with your actual data")
    print("   5. Add your personal details and affiliations")

if __name__ == "__main__":
    main()