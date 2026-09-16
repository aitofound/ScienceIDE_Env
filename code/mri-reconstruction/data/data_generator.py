"""
Synthetic MRI Data Generator
Generates synthetic MRI images for testing reconstruction algorithms
"""

import numpy as np
from typing import Tuple, Optional
import matplotlib.pyplot as plt


class SyntheticMRIGenerator:
    """Generate synthetic MRI images with various patterns and noise levels"""
    
    def __init__(self, image_size: Tuple[int, int] = (256, 256)):
        """
        Initialize the generator
        
        Args:
            image_size: Size of generated images (height, width)
        """
        self.image_size = image_size
    
    def generate_shepp_logan(self) -> np.ndarray:
        """
        Generate a Shepp-Logan phantom (standard test image for medical imaging)
        
        Returns:
            2D numpy array representing the phantom
        """
        # Simple implementation of Shepp-Logan phantom
        H, W = self.image_size
        y, x = np.ogrid[:H, :W]
        
        # Center coordinates
        cx, cy = W // 2, H // 2
        
        # Create ellipses with different intensities
        phantom = np.zeros((H, W))
        
        # Main ellipse (head outline)
        ellipse1 = ((x - cx) / (0.69 * W/2))**2 + ((y - cy) / (0.92 * H/2))**2 <= 1
        phantom[ellipse1] = 1.0
        
        # Brain matter
        ellipse2 = ((x - cx) / (0.6624 * W/2))**2 + ((y - cy) / (0.874 * H/2))**2 <= 1
        phantom[ellipse2] = -0.98
        
        # Additional smaller structures
        ellipse3 = (((x - cx + 0.22*W/2) / (0.11 * W/2))**2 + 
                   ((y - cy) / (0.31 * H/2))**2) <= 1
        phantom[ellipse3] = -0.8
        
        ellipse4 = (((x - cx - 0.22*W/2) / (0.16 * W/2))**2 + 
                   ((y - cy) / (0.41 * H/2))**2) <= 1
        phantom[ellipse4] = -0.8
        
        # Normalize to [0, 1]
        phantom = (phantom - phantom.min()) / (phantom.max() - phantom.min())
        
        return phantom
    
    def generate_brain_like(self, num_structures: int = 5) -> np.ndarray:
        """
        Generate a brain-like synthetic image with multiple structures
        
        Args:
            num_structures: Number of anatomical structures to simulate
            
        Returns:
            2D numpy array representing synthetic brain image
        """
        H, W = self.image_size
        image = np.zeros((H, W))
        
        # Create random anatomical structures
        np.random.seed(42)  # For reproducibility
        for i in range(num_structures):
            # Random ellipse parameters
            center_x = np.random.randint(W//4, 3*W//4)
            center_y = np.random.randint(H//4, 3*H//4)
            radius_x = np.random.randint(W//20, W//8)
            radius_y = np.random.randint(H//20, H//8)
            intensity = np.random.uniform(0.3, 1.0)
            
            # Create ellipse
            y, x = np.ogrid[:H, :W]
            ellipse = ((x - center_x) / radius_x)**2 + ((y - center_y) / radius_y)**2 <= 1
            image[ellipse] = intensity
        
        # Add smooth background gradient
        y, x = np.meshgrid(np.linspace(0, 1, H), np.linspace(0, 1, W), indexing='ij')
        background = 0.1 * (np.sin(2 * np.pi * x) * np.cos(2 * np.pi * y))
        image = image + background
        
        # Normalize
        image = np.clip(image, 0, 1)
        
        return image
    
    def add_noise(self, image: np.ndarray, noise_level: float = 0.05, 
                  noise_type: str = 'gaussian') -> np.ndarray:
        """
        Add noise to the image
        
        Args:
            image: Input image
            noise_level: Standard deviation of noise (relative to image range)
            noise_type: Type of noise ('gaussian', 'rician')
            
        Returns:
            Noisy image
        """
        if noise_type == 'gaussian':
            noise = np.random.normal(0, noise_level, image.shape)
            noisy_image = image + noise
        elif noise_type == 'rician':
            # Rician noise (common in MRI)
            real_part = image + np.random.normal(0, noise_level, image.shape)
            imag_part = np.random.normal(0, noise_level, image.shape)
            noisy_image = np.sqrt(real_part**2 + imag_part**2)
        else:
            raise ValueError(f"Unknown noise type: {noise_type}")
        
        return np.clip(noisy_image, 0, 1)
    
    def visualize_image(self, image: np.ndarray, title: str = "MRI Image"):
        """
        Visualize the generated image
        
        Args:
            image: Image to visualize
            title: Plot title
        """
        plt.figure(figsize=(8, 6))
        plt.imshow(image, cmap='gray')
        plt.title(title)
        plt.colorbar()
        plt.axis('off')
        plt.show()


def main():
    """Test the synthetic data generator"""
    generator = SyntheticMRIGenerator(image_size=(256, 256))
    
    # Generate different types of images
    shepp_logan = generator.generate_shepp_logan()
    brain_like = generator.generate_brain_like()
    
    # Add noise
    noisy_shepp = generator.add_noise(shepp_logan, noise_level=0.05)
    noisy_brain = generator.add_noise(brain_like, noise_level=0.03, noise_type='rician')
    
    # Visualize (comment out if running tests)
    # generator.visualize_image(shepp_logan, "Shepp-Logan Phantom")
    # generator.visualize_image(brain_like, "Brain-like Synthetic Image")
    # generator.visualize_image(noisy_shepp, "Noisy Shepp-Logan")
    # generator.visualize_image(noisy_brain, "Noisy Brain-like (Rician)")
    
    print("Synthetic data generator working correctly!")
    print(f"Shepp-Logan shape: {shepp_logan.shape}, range: [{shepp_logan.min():.3f}, {shepp_logan.max():.3f}]")
    print(f"Brain-like shape: {brain_like.shape}, range: [{brain_like.min():.3f}, {brain_like.max():.3f}]")


if __name__ == "__main__":
    main()