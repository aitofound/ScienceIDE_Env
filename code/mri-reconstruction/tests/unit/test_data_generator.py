"""
Test suite for the synthetic MRI data generator
"""

import pytest
import numpy as np
import sys
import os

# Add the project root to the path (adjust as needed)
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from data.data_generator import SyntheticMRIGenerator


class TestSyntheticMRIGenerator:
    """Test class for SyntheticMRIGenerator"""
    
    def setup_method(self):
        """Set up test fixtures before each test method"""
        self.generator = SyntheticMRIGenerator(image_size=(128, 128))
        self.small_generator = SyntheticMRIGenerator(image_size=(64, 64))
    
    def test_initialization(self):
        """Test generator initialization"""
        gen = SyntheticMRIGenerator()
        assert gen.image_size == (256, 256), "Default image size should be (256, 256)"
        
        gen_custom = SyntheticMRIGenerator(image_size=(128, 128))
        assert gen_custom.image_size == (128, 128), "Custom image size not set correctly"
    
    def test_shepp_logan_generation(self):
        """Test Shepp-Logan phantom generation"""
        phantom = self.generator.generate_shepp_logan()
        
        # Check shape
        assert phantom.shape == (128, 128), f"Expected shape (128, 128), got {phantom.shape}"
        
        # Check data type
        assert phantom.dtype == np.float64, f"Expected float64, got {phantom.dtype}"
        
        # Check value range (should be normalized to [0, 1])
        assert 0 <= phantom.min() <= phantom.max() <= 1, \
            f"Values should be in [0, 1], got range [{phantom.min():.3f}, {phantom.max():.3f}]"
        
        # Check that it's not all zeros or all ones
        assert not np.allclose(phantom, 0), "Phantom should not be all zeros"
        assert not np.allclose(phantom, 1), "Phantom should not be all ones"
        
        # Check that there's some structure (standard deviation > 0)
        assert phantom.std() > 0.1, "Phantom should have significant structure"
    
    def test_brain_like_generation(self):
        """Test brain-like image generation"""
        brain_image = self.generator.generate_brain_like(num_structures=3)
        
        # Check shape
        assert brain_image.shape == (128, 128), f"Expected shape (128, 128), got {brain_image.shape}"
        
        # Check value range
        assert 0 <= brain_image.min() <= brain_image.max() <= 1, \
            f"Values should be in [0, 1], got range [{brain_image.min():.3f}, {brain_image.max():.3f}]"
        
        # Check reproducibility (with fixed seed)
        brain_image2 = self.generator.generate_brain_like(num_structures=3)
        assert np.allclose(brain_image, brain_image2), "Results should be reproducible with fixed seed"
        
        # Check that different numbers of structures produce different results
        brain_image_diff = self.generator.generate_brain_like(num_structures=8)
        assert not np.allclose(brain_image, brain_image_diff), \
            "Different number of structures should produce different images"
    
    def test_gaussian_noise(self):
        """Test Gaussian noise addition"""
        # Generate clean image
        clean_image = self.small_generator.generate_shepp_logan()
        
        # Add noise
        noise_level = 0.1
        noisy_image = self.small_generator.add_noise(clean_image, noise_level=noise_level, 
                                                    noise_type='gaussian')
        
        # Check shape preservation
        assert noisy_image.shape == clean_image.shape, "Noise should preserve image shape"
        
        # Check that noise was actually added
        assert not np.allclose(clean_image, noisy_image), "Noise should change the image"
        
        # Check value range is still valid
        assert noisy_image.min() >= 0 and noisy_image.max() <= 1, \
            "Noisy image should still be in [0, 1] range"
        
        # Test different noise levels
        low_noise = self.small_generator.add_noise(clean_image, noise_level=0.01)
        high_noise = self.small_generator.add_noise(clean_image, noise_level=0.2)
        
        # Higher noise should create larger differences
        low_diff = np.mean((clean_image - low_noise)**2)
        high_diff = np.mean((clean_image - high_noise)**2)
        assert high_diff > low_diff, "Higher noise level should create larger differences"
    
    def test_rician_noise(self):
        """Test Rician noise addition"""
        clean_image = self.small_generator.generate_shepp_logan()
        
        # Add Rician noise
        noisy_image = self.small_generator.add_noise(clean_image, noise_level=0.1, 
                                                    noise_type='rician')
        
        # Check shape preservation
        assert noisy_image.shape == clean_image.shape, "Rician noise should preserve image shape"
        
        # Check that noise was added
        assert not np.allclose(clean_image, noisy_image), "Rician noise should change the image"
        
        # Rician noise should generally increase values (due to magnitude operation)
        assert np.mean(noisy_image) >= np.mean(clean_image), \
            "Rician noise typically increases mean intensity"
    
    def test_invalid_noise_type(self):
        """Test invalid noise type handling"""
        clean_image = self.small_generator.generate_shepp_logan()
        
        with pytest.raises(ValueError, match="Unknown noise type"):
            self.small_generator.add_noise(clean_image, noise_type='invalid_noise')
    
    def test_different_image_sizes(self):
        """Test generation with different image sizes"""
        sizes = [(32, 32), (64, 128), (100, 100)]
        
        for size in sizes:
            gen = SyntheticMRIGenerator(image_size=size)
            phantom = gen.generate_shepp_logan()
            brain = gen.generate_brain_like()
            
            assert phantom.shape == size, f"Phantom shape {phantom.shape} != expected {size}"
            assert brain.shape == size, f"Brain image shape {brain.shape} != expected {size}"
    
    def test_edge_cases(self):
        """Test edge cases and boundary conditions"""
        # Test with very small image
        tiny_gen = SyntheticMRIGenerator(image_size=(4, 4))
        tiny_phantom = tiny_gen.generate_shepp_logan()
        assert tiny_phantom.shape == (4, 4), "Should handle very small images"
        
        # Test with zero noise
        clean_image = self.small_generator.generate_shepp_logan()
        zero_noise = self.small_generator.add_noise(clean_image, noise_level=0.0)
        assert np.allclose(clean_image, zero_noise), "Zero noise should not change image"
        
        # Test with very high noise
        high_noise = self.small_generator.add_noise(clean_image, noise_level=1.0)
        assert high_noise.min() >= 0 and high_noise.max() <= 1, \
            "Even high noise should keep values in valid range"


def run_all_tests():
    """Run all tests manually (useful for development)"""
    test_instance = TestSyntheticMRIGenerator()
    test_methods = [method for method in dir(test_instance) if method.startswith('test_')]
    
    print("Running data generator tests...")
    test_instance.setup_method()
    
    passed = 0
    failed = 0
    
    for test_method in test_methods:
        try:
            print(f"  Running {test_method}...", end="")
            getattr(test_instance, test_method)()
            print(" ✓ PASSED")
            passed += 1
        except Exception as e:
            print(f" ✗ FAILED: {str(e)}")
            failed += 1
    
    print(f"\nResults: {passed} passed, {failed} failed")
    return failed == 0


if __name__ == "__main__":
    # Run tests manually
    success = run_all_tests()
    if success:
        print("\n🎉 All tests passed! Data generator is ready.")
    else:
        print("\n❌ Some tests failed. Please fix the issues before proceeding.")
        sys.exit(1)