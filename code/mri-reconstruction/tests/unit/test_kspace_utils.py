"""
Test suite for k-space utilities
"""

import pytest
import numpy as np
import sys
import os

# Add the project root to the path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from algorithms.utils.kspace import KSpaceUtils
from data.data_generator import SyntheticMRIGenerator


class TestKSpaceUtils:
    """Test class for KSpaceUtils"""
    
    def setup_method(self):
        """Set up test fixtures before each test method"""
        self.generator = SyntheticMRIGenerator(image_size=(64, 64))
        self.test_image = self.generator.generate_shepp_logan()
        self.small_image = np.random.rand(8, 8)
    
    def test_fft_round_trip(self):
        """Test that FFT and IFFT are proper inverses"""
        # Test with real image
        kspace = KSpaceUtils.fft2c(self.test_image)
        reconstructed = KSpaceUtils.ifft2c(kspace)
        
        # Should reconstruct original image (up to numerical precision)
        error = np.mean(np.abs(self.test_image - np.abs(reconstructed))**2)
        assert error < 1e-20, f"FFT round-trip error too large: {error}"
        
        # Test with complex data
        complex_image = self.test_image + 1j * self.test_image * 0.5
        kspace_complex = KSpaceUtils.fft2c(complex_image)
        reconstructed_complex = KSpaceUtils.ifft2c(kspace_complex)
        
        error_complex = np.mean(np.abs(complex_image - reconstructed_complex)**2)
        assert error_complex < 1e-20, f"Complex FFT round-trip error too large: {error_complex}"
    
    def test_fft_properties(self):
        """Test mathematical properties of FFT"""
        kspace = KSpaceUtils.fft2c(self.test_image)
        
        # K-space should be complex
        assert np.iscomplexobj(kspace), "K-space should be complex"
        
        # Check Parseval's theorem (energy conservation)
        image_energy = np.sum(np.abs(self.test_image)**2)
        kspace_energy = np.sum(np.abs(kspace)**2) / kspace.size
        
        relative_error = abs(image_energy - kspace_energy) / image_energy
        assert relative_error < 1e-10, f"Energy not conserved: {relative_error}"
        
        # Test scaling
        scaled_image = 2.0 * self.test_image
        scaled_kspace = KSpaceUtils.fft2c(scaled_image)
        expected_kspace = 2.0 * kspace
        
        assert np.allclose(scaled_kspace, expected_kspace), "FFT should be linear"
    
    def test_sampling_mask_creation(self):
        """Test different sampling mask patterns"""
        shape = (32, 32)
        acceleration = 4.0
        
        # Test random pattern
        mask_random = KSpaceUtils.create_sampling_mask(
            shape, acceleration_factor=acceleration, pattern='random'
        )
        
        assert mask_random.shape == shape, "Mask shape should match input"
        assert mask_random.dtype == bool, "Mask should be boolean"
        
        sampling_ratio = np.sum(mask_random) / mask_random.size
        expected_ratio = 1.0 / acceleration
        
        # Should be approximately correct (within 20% due to center fraction)
        assert 0.8 * expected_ratio <= sampling_ratio <= 1.5 * expected_ratio, \
            f"Sampling ratio {sampling_ratio:.3f} not close to expected {expected_ratio:.3f}"
        
        # Test 1D random pattern
        mask_1d = KSpaceUtils.create_sampling_mask(
            shape, acceleration_factor=acceleration, pattern='1d_random'
        )
        
        # Should have full lines (not isolated points)
        sampled_lines = np.any(mask_1d, axis=1)
        for i in range(shape[0]):
            if sampled_lines[i]:
                assert np.all(mask_1d[i, :]), f"Line {i} should be fully sampled or not sampled"
        
        # Test uniform pattern
        mask_uniform = KSpaceUtils.create_sampling_mask(
            shape, acceleration_factor=acceleration, pattern='uniform'
        )
        
        # Should have regular spacing
        sampled_lines = np.where(np.any(mask_uniform, axis=1))[0]
        if len(sampled_lines) > 1:
            spacings = np.diff(sampled_lines)
            # Most spacings should be similar (uniform)
            assert np.std(spacings) / np.mean(spacings) < 0.5, "Uniform pattern should have regular spacing"
    
    def test_center_sampling(self):
        """Test that center k-space is always fully sampled"""
        shape = (64, 64)
        center_fraction = 0.1
        
        mask = KSpaceUtils.create_sampling_mask(
            shape, acceleration_factor=4.0, center_fraction=center_fraction
        )
        
        # Calculate center region
        H, W = shape
        center_lines = int(center_fraction * H)
        center_start = H // 2 - center_lines // 2
        center_end = center_start + center_lines
        
        # Center should be fully sampled
        center_region = mask[center_start:center_end, :]
        assert np.all(center_region), "Center k-space should be fully sampled"
    
    def test_apply_sampling_mask(self):
        """Test application of sampling mask"""
        kspace = KSpaceUtils.fft2c(self.test_image)
        mask = KSpaceUtils.create_sampling_mask(kspace.shape, acceleration_factor=2.0)
        
        undersampled = KSpaceUtils.apply_sampling_mask(kspace, mask)
        
        # Check that masked regions are zero
        assert np.allclose(undersampled[~mask], 0), "Unsampled regions should be zero"
        
        # Check that sampled regions are preserved
        assert np.allclose(undersampled[mask], kspace[mask]), "Sampled regions should be preserved"
        
        # Shape should be preserved
        assert undersampled.shape == kspace.shape, "Shape should be preserved"
    
    def test_noise_addition(self):
        """Test k-space noise addition"""
        kspace = KSpaceUtils.fft2c(self.test_image)
        
        # Test different SNR levels
        snr_high = 40.0
        snr_low = 10.0
        
        noisy_high = KSpaceUtils.add_noise_to_kspace(kspace, snr_db=snr_high)
        noisy_low = KSpaceUtils.add_noise_to_kspace(kspace, snr_db=snr_low)
        
        # Higher SNR should have less noise
        noise_high = np.mean(np.abs(kspace - noisy_high)**2)
        noise_low = np.mean(np.abs(kspace - noisy_low)**2)
        
        assert noise_low > noise_high, "Lower SNR should have more noise"
        
        # Noisy k-space should be complex
        assert np.iscomplexobj(noisy_high), "Noisy k-space should be complex"
        assert np.iscomplexobj(noisy_low), "Noisy k-space should be complex"
        
        # Shape should be preserved
        assert noisy_high.shape == kspace.shape, "Shape should be preserved"
    
    def test_zero_fill_reconstruction(self):
        """Test zero-filled reconstruction"""
        kspace = KSpaceUtils.fft2c(self.test_image)
        mask = KSpaceUtils.create_sampling_mask(kspace.shape, acceleration_factor=2.0)
        undersampled = KSpaceUtils.apply_sampling_mask(kspace, mask)
        
        reconstruction = KSpaceUtils.zero_fill_reconstruction(undersampled)
        
        # Should be real and positive (magnitude image)
        assert np.all(reconstruction >= 0), "Reconstruction should be non-negative"
        assert np.all(np.isreal(reconstruction)), "Reconstruction should be real"
        
        # Shape should match original
        assert reconstruction.shape == self.test_image.shape, "Shape should be preserved"
        
        # Should be different from original (due to undersampling)
        mse = np.mean((self.test_image - reconstruction)**2)
        assert mse > 1e-10, "Undersampled reconstruction should differ from original"
        
        # But should still have some similarity
        correlation = np.corrcoef(self.test_image.flatten(), reconstruction.flatten())[0, 1]
        assert correlation > 0.5, f"Reconstruction should correlate with original: {correlation:.3f}"
    
    def test_sampling_percentage_calculation(self):
        """Test sampling percentage calculation"""
        # Test full sampling
        full_mask = np.ones((10, 10), dtype=bool)
        assert KSpaceUtils.calculate_sampling_percentage(full_mask) == 100.0
        
        # Test no sampling
        zero_mask = np.zeros((10, 10), dtype=bool)
        assert KSpaceUtils.calculate_sampling_percentage(zero_mask) == 0.0
        
        # Test partial sampling
        partial_mask = np.zeros((10, 10), dtype=bool)
        partial_mask[:5, :] = True  # 50% sampling
        assert KSpaceUtils.calculate_sampling_percentage(partial_mask) == 50.0
    
    def test_edge_cases(self):
        """Test edge cases and boundary conditions"""
        # Test with very small image
        tiny_image = np.random.rand(2, 2)
        tiny_kspace = KSpaceUtils.fft2c(tiny_image)
        tiny_reconstructed = KSpaceUtils.ifft2c(tiny_kspace)
        
        error = np.mean(np.abs(tiny_image - np.abs(tiny_reconstructed))**2)
        assert error < 1e-20, "Should work with very small images"
        
        # Test with single pixel
        single_pixel = np.array([[1.0]])
        single_kspace = KSpaceUtils.fft2c(single_pixel)
        single_reconstructed = KSpaceUtils.ifft2c(single_kspace)
        
        assert np.allclose(single_pixel, np.abs(single_reconstructed)), "Should work with single pixel"
        
        # Test zero image
        zero_image = np.zeros((4, 4))
        zero_kspace = KSpaceUtils.fft2c(zero_image)
        assert np.allclose(zero_kspace, 0), "Zero image should give zero k-space"
    
    def test_mask_reproducibility(self):
        """Test that masks are reproducible with fixed seed"""
        shape = (32, 32)
        
        mask1 = KSpaceUtils.create_sampling_mask(shape, acceleration_factor=4.0, pattern='random')
        mask2 = KSpaceUtils.create_sampling_mask(shape, acceleration_factor=4.0, pattern='random')
        
        assert np.array_equal(mask1, mask2), "Masks should be reproducible with fixed seed"


def run_all_tests():
    """Run all tests manually"""
    test_instance = TestKSpaceUtils()
    test_methods = [method for method in dir(test_instance) if method.startswith('test_')]
    
    print("Running k-space utilities tests...")
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
    success = run_all_tests()
    if success:
        print("\n🎉 All k-space tests passed! Ready for next component.")
    else:
        print("\n❌ Some tests failed. Please fix issues before proceeding.")
        sys.exit(1)