"""
K-space utilities for MRI reconstruction
Handles Fourier transforms, k-space sampling, and related operations
"""

import numpy as np
from typing import Tuple, Optional, Union
import matplotlib.pyplot as plt


class KSpaceUtils:
    """Utilities for k-space operations in MRI reconstruction"""
    
    @staticmethod
    def fft2c(image: np.ndarray) -> np.ndarray:
        """
        Centered 2D FFT (image to k-space)
        
        Args:
            image: 2D image array
            
        Returns:
            K-space data (complex array)
        """
        # Apply fftshift before FFT for proper centering
        return np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(image)))
    
    @staticmethod
    def ifft2c(kspace: np.ndarray) -> np.ndarray:
        """
        Centered 2D IFFT (k-space to image)
        
        Args:
            kspace: K-space data (complex array)
            
        Returns:
            Reconstructed image (complex array)
        """
        # Apply fftshift before IFFT for proper centering
        return np.fft.fftshift(np.fft.ifft2(np.fft.ifftshift(kspace)))
    
    @staticmethod
    def create_sampling_mask(shape: Tuple[int, int], 
                           acceleration_factor: float = 4.0,
                           center_fraction: float = 0.08,
                           pattern: str = 'random') -> np.ndarray:
        """
        Create undersampling mask for compressed sensing
        
        Args:
            shape: Shape of k-space data (height, width)
            acceleration_factor: Undersampling acceleration factor
            center_fraction: Fraction of center k-space to fully sample
            pattern: Sampling pattern ('random', '1d_random', 'uniform')
            
        Returns:
            Binary sampling mask
        """
        H, W = shape
        mask = np.zeros((H, W), dtype=bool)
        
        # Always fully sample the center (low frequencies are crucial)
        center_lines = int(center_fraction * H)
        center_start = H // 2 - center_lines // 2
        center_end = center_start + center_lines
        mask[center_start:center_end, :] = True
        
        # Calculate remaining sampling
        total_samples_needed = int(H * W / acceleration_factor)
        center_samples = center_lines * W
        remaining_samples = max(0, total_samples_needed - center_samples)
        
        if pattern == 'random':
            # Random sampling in remaining k-space
            remaining_positions = []
            for i in range(H):
                for j in range(W):
                    if not mask[i, j]:
                        remaining_positions.append((i, j))
            
            if remaining_samples > 0 and len(remaining_positions) > 0:
                np.random.seed(42)  # For reproducibility
                sampled_positions = np.random.choice(
                    len(remaining_positions), 
                    min(remaining_samples, len(remaining_positions)), 
                    replace=False
                )
                for idx in sampled_positions:
                    i, j = remaining_positions[idx]
                    mask[i, j] = True
                    
        elif pattern == '1d_random':
            # Random sampling along phase encoding direction (more realistic)
            lines_to_sample = remaining_samples // W
            available_lines = [i for i in range(H) if not np.any(mask[i, :])]
            
            if lines_to_sample > 0 and len(available_lines) > 0:
                np.random.seed(42)
                sampled_lines = np.random.choice(
                    available_lines,
                    min(lines_to_sample, len(available_lines)),
                    replace=False
                )
                for line in sampled_lines:
                    mask[line, :] = True
                    
        elif pattern == 'uniform':
            # Uniform undersampling
            step = int(acceleration_factor)
            for i in range(0, H, step):
                if not np.any(mask[i, :]):
                    mask[i, :] = True
        
        return mask
    
    @staticmethod
    def apply_sampling_mask(kspace: np.ndarray, mask: np.ndarray) -> np.ndarray:
        """
        Apply sampling mask to k-space data
        
        Args:
            kspace: Full k-space data
            mask: Binary sampling mask
            
        Returns:
            Undersampled k-space data
        """
        return kspace * mask
    
    @staticmethod
    def add_noise_to_kspace(kspace: np.ndarray, snr_db: float = 30.0) -> np.ndarray:
        """
        Add complex Gaussian noise to k-space data
        
        Args:
            kspace: K-space data
            snr_db: Signal-to-noise ratio in dB
            
        Returns:
            Noisy k-space data
        """
        # Calculate noise power based on signal power and desired SNR
        signal_power = np.mean(np.abs(kspace)**2)
        noise_power = signal_power / (10**(snr_db / 10))
        noise_std = np.sqrt(noise_power / 2)  # Divide by 2 for complex noise
        
        # Add complex Gaussian noise
        noise_real = np.random.normal(0, noise_std, kspace.shape)
        noise_imag = np.random.normal(0, noise_std, kspace.shape)
        noise = noise_real + 1j * noise_imag
        
        return kspace + noise
    
    @staticmethod
    def zero_fill_reconstruction(undersampled_kspace: np.ndarray) -> np.ndarray:
        """
        Simple zero-filled reconstruction (baseline method)
        
        Args:
            undersampled_kspace: Undersampled k-space data
            
        Returns:
            Zero-filled reconstruction (magnitude image)
        """
        # Direct IFFT of undersampled data
        complex_image = KSpaceUtils.ifft2c(undersampled_kspace)
        return np.abs(complex_image)
    
    @staticmethod
    def calculate_sampling_percentage(mask: np.ndarray) -> float:
        """
        Calculate the percentage of k-space that is sampled
        
        Args:
            mask: Binary sampling mask
            
        Returns:
            Sampling percentage (0-100)
        """
        return np.sum(mask) / mask.size * 100
    
    @staticmethod
    def visualize_kspace(kspace: np.ndarray, title: str = "K-space Data", 
                        log_scale: bool = True):
        """
        Visualize k-space data
        
        Args:
            kspace: K-space data (can be complex)
            title: Plot title
            log_scale: Whether to use log scale for better visualization
        """
        plt.figure(figsize=(12, 4))
        
        # Magnitude
        plt.subplot(1, 3, 1)
        mag = np.abs(kspace)
        if log_scale:
            mag = np.log(mag + 1e-8)  # Add small epsilon to avoid log(0)
            plt.title(f"{title} - Log Magnitude")
        else:
            plt.title(f"{title} - Magnitude")
        plt.imshow(mag, cmap='gray')
        plt.colorbar()
        plt.axis('off')
        
        # Phase (if complex)
        plt.subplot(1, 3, 2)
        if np.iscomplexobj(kspace):
            plt.imshow(np.angle(kspace), cmap='hsv')
            plt.title(f"{title} - Phase")
        else:
            plt.imshow(kspace, cmap='gray')
            plt.title(f"{title} - Real Part")
        plt.colorbar()
        plt.axis('off')
        
        # Real part
        plt.subplot(1, 3, 3)
        plt.imshow(np.real(kspace), cmap='gray')
        plt.title(f"{title} - Real Part")
        plt.colorbar()
        plt.axis('off')
        
        plt.tight_layout()
        plt.show()
    
    @staticmethod
    def visualize_sampling_mask(mask: np.ndarray, title: str = "Sampling Mask"):
        """
        Visualize sampling mask
        
        Args:
            mask: Binary sampling mask
            title: Plot title
        """
        plt.figure(figsize=(8, 6))
        plt.imshow(mask, cmap='gray')
        plt.title(f"{title} - Sampling: {KSpaceUtils.calculate_sampling_percentage(mask):.1f}%")
        plt.colorbar()
        plt.axis('off')
        plt.show()


def main():
    """Test the k-space utilities"""
    from data.data_generator import SyntheticMRIGenerator
    
    # Generate test image
    generator = SyntheticMRIGenerator(image_size=(128, 128))
    test_image = generator.generate_shepp_logan()
    
    print("Testing K-space utilities...")
    
    # Test forward and inverse FFT
    kspace = KSpaceUtils.fft2c(test_image)
    reconstructed = KSpaceUtils.ifft2c(kspace)
    reconstruction_error = np.mean(np.abs(test_image - np.abs(reconstructed))**2)
    print(f"FFT round-trip error: {reconstruction_error:.2e}")
    
    # Test sampling masks
    mask_random = KSpaceUtils.create_sampling_mask((128, 128), acceleration_factor=4.0, pattern='random')
    mask_1d = KSpaceUtils.create_sampling_mask((128, 128), acceleration_factor=4.0, pattern='1d_random')
    mask_uniform = KSpaceUtils.create_sampling_mask((128, 128), acceleration_factor=4.0, pattern='uniform')
    
    print(f"Random mask sampling: {KSpaceUtils.calculate_sampling_percentage(mask_random):.1f}%")
    print(f"1D random mask sampling: {KSpaceUtils.calculate_sampling_percentage(mask_1d):.1f}%")
    print(f"Uniform mask sampling: {KSpaceUtils.calculate_sampling_percentage(mask_uniform):.1f}%")
    
    # Test undersampling and reconstruction
    undersampled_kspace = KSpaceUtils.apply_sampling_mask(kspace, mask_random)
    zero_filled = KSpaceUtils.zero_fill_reconstruction(undersampled_kspace)
    
    # Calculate reconstruction quality
    mse = np.mean((test_image - zero_filled)**2)
    psnr = 20 * np.log10(1.0 / np.sqrt(mse))
    print(f"Zero-filled reconstruction PSNR: {psnr:.2f} dB")
    
    # Test noise addition
    noisy_kspace = KSpaceUtils.add_noise_to_kspace(kspace, snr_db=20.0)
    noisy_reconstruction = np.abs(KSpaceUtils.ifft2c(noisy_kspace))
    noise_mse = np.mean((test_image - noisy_reconstruction)**2)
    noise_psnr = 20 * np.log10(1.0 / np.sqrt(noise_mse))
    print(f"Noisy reconstruction PSNR: {noise_psnr:.2f} dB")
    
    print("K-space utilities working correctly!")


if __name__ == "__main__":
    main()