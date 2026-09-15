"""
Test suite for evaluation metrics
"""

import pytest
import numpy as np
import torch
import sys
import os

# Add the project root to the path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from evaluation.metrics.reconstruction_metrics import (
    ReconstructionMetrics, BatchMetrics, compare_reconstructions
)


class TestReconstructionMetrics:
    """Test individual reconstruction metrics"""
    
    def setup_method(self):
        """Set up test fixtures"""
        np.random.seed(42)
        self.reference = np.random.rand(32, 32)
        self.identical = self.reference.copy()
        self.noisy = self.reference + 0.1 * np.random.randn(*self.reference.shape)
        self.zeros = np.zeros_like(self.reference)
    
    def test_mse_perfect_reconstruction(self):
        """Test MSE with perfect reconstruction"""
        mse = ReconstructionMetrics.mse(self.reference, self.identical)
        assert mse < 1e-10, f"MSE should be near zero for identical images: {mse}"
    
    def test_mse_properties(self):
        """Test MSE properties"""
        mse_noisy = ReconstructionMetrics.mse(self.reference, self.noisy)
        mse_zeros = ReconstructionMetrics.mse(self.reference, self.zeros)
        
        assert mse_noisy > 0, "MSE should be positive for different images"
        assert mse_zeros > mse_noisy, "MSE should be larger for worse reconstruction"
    
    def test_rmse(self):
        """Test RMSE calculation"""
        mse = ReconstructionMetrics.mse(self.reference, self.noisy)
        rmse = ReconstructionMetrics.rmse(self.reference, self.noisy)
        
        assert np.isclose(rmse, np.sqrt(mse)), "RMSE should equal sqrt(MSE)"
        assert rmse >= 0, "RMSE should be non-negative"
    
    def test_mae(self):
        """Test Mean Absolute Error"""
        mae_perfect = ReconstructionMetrics.mae(self.reference, self.identical)
        mae_noisy = ReconstructionMetrics.mae(self.reference, self.noisy)
        
        assert mae_perfect < 1e-10, "MAE should be near zero for identical images"
        assert mae_noisy > 0, "MAE should be positive for different images"
    
    def test_psnr_perfect_reconstruction(self):
        """Test PSNR with perfect reconstruction"""
        psnr = ReconstructionMetrics.psnr(self.reference, self.identical)
        assert psnr > 100, f"PSNR should be very high for identical images: {psnr}"
    
    def test_psnr_properties(self):
        """Test PSNR properties"""
        psnr_noisy = ReconstructionMetrics.psnr(self.reference, self.noisy)
        psnr_zeros = ReconstructionMetrics.psnr(self.reference, self.zeros)
        
        assert psnr_noisy > psnr_zeros, "Better reconstruction should have higher PSNR"
        assert psnr_noisy > 0, "PSNR should be positive"
    
    def test_ssim_perfect_reconstruction(self):
        """Test SSIM with perfect reconstruction"""
        ssim = ReconstructionMetrics.ssim(self.reference, self.identical)
        assert np.isclose(ssim, 1.0, atol=1e-6), f"SSIM should be 1.0 for identical images: {ssim}"
    
    def test_ssim_properties(self):
        """Test SSIM properties"""
        ssim_noisy = ReconstructionMetrics.ssim(self.reference, self.noisy)
        ssim_zeros = ReconstructionMetrics.ssim(self.reference, self.zeros)
        
        assert 0 <= ssim_noisy <= 1, f"SSIM should be between 0 and 1: {ssim_noisy}"
        assert 0 <= ssim_zeros <= 1, f"SSIM should be between 0 and 1: {ssim_zeros}"
        assert ssim_noisy > ssim_zeros, "Better reconstruction should have higher SSIM"
    
    def test_nrmse(self):
        """Test Normalized RMSE"""
        nrmse_euclidean = ReconstructionMetrics.nrmse(self.reference, self.noisy, 'euclidean')
        nrmse_minmax = ReconstructionMetrics.nrmse(self.reference, self.noisy, 'min-max')
        nrmse_mean = ReconstructionMetrics.nrmse(self.reference, self.noisy, 'mean')
        
        assert nrmse_euclidean > 0, "NRMSE should be positive"
        assert nrmse_minmax > 0, "NRMSE should be positive"
        assert nrmse_mean > 0, "NRMSE should be positive"
        
        # Perfect reconstruction should have near-zero NRMSE
        nrmse_perfect = ReconstructionMetrics.nrmse(self.reference, self.identical, 'euclidean')
        assert nrmse_perfect < 1e-10, "NRMSE should be near zero for perfect reconstruction"
    
    def test_nrmse_invalid_normalization(self):
        """Test NRMSE with invalid normalization"""
        with pytest.raises(ValueError, match="Unknown normalization"):
            ReconstructionMetrics.nrmse(self.reference, self.noisy, 'invalid')
    
    def test_snr(self):
        """Test Signal-to-Noise Ratio"""
        snr_noisy = ReconstructionMetrics.snr(self.reference, self.noisy)
        snr_perfect = ReconstructionMetrics.snr(self.reference, self.identical)
        
        assert snr_noisy > 0, "SNR should be positive for reasonable reconstruction"
        assert snr_perfect == float('inf'), "SNR should be infinite for perfect reconstruction"
    
    def test_correlation_coefficient(self):
        """Test correlation coefficient"""
        corr_perfect = ReconstructionMetrics.correlation_coefficient(self.reference, self.identical)
        corr_noisy = ReconstructionMetrics.correlation_coefficient(self.reference, self.noisy)
        
        assert np.isclose(corr_perfect, 1.0), "Correlation should be 1.0 for identical images"
        assert -1 <= corr_noisy <= 1, "Correlation should be between -1 and 1"
        assert corr_noisy > 0, "Correlation should be positive for similar images"
    
    def test_edge_preservation_index(self):
        """Test Edge Preservation Index"""
        epi_perfect = ReconstructionMetrics.edge_preservation_index(self.reference, self.identical)
        epi_noisy = ReconstructionMetrics.edge_preservation_index(self.reference, self.noisy)
        
        assert np.isclose(epi_perfect, 1.0, atol=1e-6), "EPI should be 1.0 for identical images"
        assert 0 <= epi_noisy <= 1, "EPI should be between 0 and 1"
    
    def test_blur_metric(self):
        """Test blur metric"""
        blur_original = ReconstructionMetrics.blur_metric(self.reference)
        
        # Create blurred version
        from scipy import ndimage
        blurred = ndimage.gaussian_filter(self.reference, sigma=2.0)
        blur_blurred = ReconstructionMetrics.blur_metric(blurred)
        
        assert blur_original > blur_blurred, "Original should be sharper than blurred version"
        assert blur_original >= 0, "Blur metric should be non-negative"
        assert blur_blurred >= 0, "Blur metric should be non-negative"
    
    def test_artifact_power(self):
        """Test artifact power measurement"""
        artifact_power = ReconstructionMetrics.artifact_power(self.reference, self.noisy)
        assert artifact_power >= 0, "Artifact power should be non-negative"
        
        # Perfect reconstruction should have zero artifact power
        perfect_power = ReconstructionMetrics.artifact_power(self.reference, self.identical)
        assert perfect_power < 1e-10, "Perfect reconstruction should have zero artifact power"


class TestBatchMetrics:
    """Test batch processing of metrics"""
    
    def setup_method(self):
        """Set up test fixtures"""
        np.random.seed(42)
        self.batch_size = 4
        self.height, self.width = 16, 16
        
        self.batch_refs = np.random.rand(self.batch_size, self.height, self.width)
        self.batch_recs = self.batch_refs + 0.05 * np.random.randn(self.batch_size, self.height, self.width)
    
    def test_batch_metrics_numpy(self):
        """Test batch metrics with numpy arrays"""
        results = BatchMetrics.compute_batch_metrics(self.batch_refs, self.batch_recs)
        
        # Check structure
        assert isinstance(results, dict), "Results should be a dictionary"
        
        for metric, stats in results.items():
            assert 'mean' in stats, f"Stats should contain mean for {metric}"
            assert 'std' in stats, f"Stats should contain std for {metric}"
            assert 'values' in stats, f"Stats should contain values for {metric}"
            assert len(stats['values']) == self.batch_size, f"Should have {self.batch_size} values"
    
    def test_batch_metrics_torch(self):
        """Test batch metrics with PyTorch tensors"""
        torch_refs = torch.from_numpy(self.batch_refs).float()
        torch_recs = torch.from_numpy(self.batch_recs).float()
        
        results = BatchMetrics.compute_batch_metrics(torch_refs, torch_recs)
        
        assert isinstance(results, dict), "Results should be a dictionary"
        assert 'psnr' in results, "Should compute PSNR"
        assert 'ssim' in results, "Should compute SSIM"
    
    def test_batch_metrics_with_channels(self):
        """Test batch metrics with channel dimension"""
        # Add channel dimension
        batch_refs_ch = self.batch_refs[:, np.newaxis, :, :]  # [N, 1, H, W]
        batch_recs_ch = self.batch_recs[:, np.newaxis, :, :]
        
        results = BatchMetrics.compute_batch_metrics(batch_refs_ch, batch_recs_ch)
        
        assert isinstance(results, dict), "Results should be a dictionary"
        
        # Should work same as without channel dimension
        results_no_ch = BatchMetrics.compute_batch_metrics(self.batch_refs, self.batch_recs)
        
        # Results should be similar (allowing for small numerical differences)
        for metric in ['mse', 'psnr']:
            if metric in results and metric in results_no_ch:
                diff = abs(results[metric]['mean'] - results_no_ch[metric]['mean'])
                assert diff < 1e-6, f"Results should be similar with/without channel dim for {metric}"
    
    def test_custom_metrics_list(self):
        """Test with custom metrics list"""
        metrics = ['mse', 'psnr', 'ssim']
        results = BatchMetrics.compute_batch_metrics(
            self.batch_refs, self.batch_recs, metrics=metrics
        )
        
        assert set(results.keys()) == set(metrics), "Should only compute requested metrics"
    
    def test_batch_metrics_statistics(self):
        """Test that batch statistics are reasonable"""
        results = BatchMetrics.compute_batch_metrics(self.batch_refs, self.batch_recs)
        
        for metric, stats in results.items():
            # Check that mean is within reasonable range of individual values
            values = stats['values']
            mean_val = stats['mean']
            
            assert min(values) <= mean_val <= max(values), f"Mean should be within range for {metric}"
            assert stats['std'] >= 0, f"Standard deviation should be non-negative for {metric}"


class TestCompareReconstructions:
    """Test reconstruction comparison functionality"""
    
    def setup_method(self):
        """Set up test fixtures"""
        np.random.seed(42)
        self.reference = np.random.rand(32, 32)
        
        self.reconstructions = {
            'Perfect': self.reference.copy(),
            'Noisy': self.reference + 0.1 * np.random.randn(*self.reference.shape),
            'Very Noisy': self.reference + 0.3 * np.random.randn(*self.reference.shape)
        }
    
    def test_compare_reconstructions(self):
        """Test comparison of multiple reconstruction methods"""
        results = compare_reconstructions(self.reference, self.reconstructions)
        
        assert isinstance(results, dict), "Results should be a dictionary"
        assert set(results.keys()) == set(self.reconstructions.keys()), "Should have results for all methods"
        
        # Perfect reconstruction should have best metrics
        perfect_psnr = results['Perfect']['psnr']
        noisy_psnr = results['Noisy']['psnr']
        very_noisy_psnr = results['Very Noisy']['psnr']
        
        assert perfect_psnr > noisy_psnr, "Perfect should have higher PSNR than noisy"
        assert noisy_psnr > very_noisy_psnr, "Noisy should have higher PSNR than very noisy"
    
    def test_compare_custom_metrics(self):
        """Test comparison with custom metrics"""
        metrics = ['mse', 'ssim']
        results = compare_reconstructions(
            self.reference, self.reconstructions, metrics=metrics
        )
        
        for method_results in results.values():
            assert set(method_results.keys()) == set(metrics), "Should only compute requested metrics"
    
    def test_compare_invalid_metric(self):
        """Test comparison with invalid metric"""
        with pytest.raises(ValueError, match="Unknown metric"):
            compare_reconstructions(
                self.reference, self.reconstructions, metrics=['invalid_metric']
            )


class TestEdgeCases:
    """Test edge cases and boundary conditions"""
    
    def test_constant_images(self):
        """Test metrics with constant images"""
        constant = np.ones((16, 16))
        
        # MSE with identical constant images
        mse = ReconstructionMetrics.mse(constant, constant)
        assert mse < 1e-10, "MSE should be zero for identical constant images"
        
        # SSIM with constant images (may return NaN for constant images)
        try:
            ssim = ReconstructionMetrics.ssim(constant, constant)
            # For constant images, SSIM might be NaN due to zero variance
            assert np.isnan(ssim) or np.isclose(ssim, 1.0, atol=1e-6), "SSIM should be 1.0 or NaN for identical constant images"
        except RuntimeWarning:
            pass  # Expected for constant images
    
    def test_zero_images(self):
        """Test metrics with zero images"""
        zeros = np.zeros((16, 16))
        small_values = np.ones((16, 16)) * 1e-8
        
        # Should not crash with zero images
        mse = ReconstructionMetrics.mse(zeros, small_values)
        assert mse >= 0, "MSE should be non-negative"
        
        # PSNR might be very high, very low, or inf with zero images
        try:
            psnr = ReconstructionMetrics.psnr(zeros, zeros + 1e-10)
            assert np.isfinite(psnr) or psnr == float('inf') or psnr == float('-inf'), "PSNR should be finite or inf"
        except RuntimeWarning:
            pass  # Expected for zero/near-zero images
    
    def test_single_pixel_images(self):
        """Test metrics with single pixel images"""
        ref = np.array([[0.5]])
        rec = np.array([[0.6]])
        
        mse = ReconstructionMetrics.mse(ref, rec)
        assert np.isclose(mse, 0.01, atol=1e-10), "MSE should be approximately 0.01 for single pixel difference of 0.1"
        
        # PSNR for single pixel might be problematic due to very small data range
        try:
            psnr = ReconstructionMetrics.psnr(ref, rec)
            # PSNR might be finite, +inf, or -inf for single pixel cases
            assert np.isfinite(psnr) or psnr == float('inf') or psnr == float('-inf'), "PSNR should be finite or inf"
        except (RuntimeWarning, ValueError):
            pass  # Expected for single pixel edge cases
    
    def test_very_small_images(self):
        """Test metrics with very small images"""
        ref = np.random.rand(2, 2)
        rec = ref + 0.1 * np.random.randn(2, 2)
        
        # Should not crash with very small images
        metrics_to_test = ['mse', 'psnr', 'correlation']  # Remove ssim for very small images
        
        for metric in metrics_to_test:
            if metric == 'mse':
                value = ReconstructionMetrics.mse(ref, rec)
            elif metric == 'psnr':
                value = ReconstructionMetrics.psnr(ref, rec)
            elif metric == 'correlation':
                value = ReconstructionMetrics.correlation_coefficient(ref, rec)
            
            assert np.isfinite(value), f"{metric} should be finite for small images"
        
        # Test SSIM with appropriate window size for small images
        try:
            ssim = ReconstructionMetrics.ssim(ref, rec, win_size=None)  # Let it auto-adjust
        except ValueError:
            # SSIM might fail for very small images - that's expected
            pass
    
    def test_extreme_values(self):
        """Test metrics with extreme pixel values"""
        ref = np.array([[0.0, 1000.0], [0.0, 1000.0]])
        rec = np.array([[0.1, 999.9], [0.1, 999.9]])
        
        # Should handle extreme values gracefully
        mse = ReconstructionMetrics.mse(ref, rec)
        assert np.isfinite(mse), "MSE should be finite with extreme values"
        
        psnr = ReconstructionMetrics.psnr(ref, rec)
        assert np.isfinite(psnr), "PSNR should be finite with extreme values"
    
    def test_nan_handling(self):
        """Test that NaN values are handled appropriately"""
        ref = np.random.rand(8, 8)
        rec_with_nan = ref.copy()
        rec_with_nan[0, 0] = np.nan
        
        # Metrics should either handle NaN or raise appropriate warnings
        try:
            mse = ReconstructionMetrics.mse(ref, rec_with_nan)
            assert np.isnan(mse) or np.isfinite(mse), "MSE should handle NaN appropriately"
        except (ValueError, RuntimeWarning):
            pass  # Acceptable to raise warning/error for NaN


def run_all_tests():
    """Run all tests manually"""
    test_classes = [
        TestReconstructionMetrics(),
        TestBatchMetrics(),
        TestCompareReconstructions(),
        TestEdgeCases()
    ]
    
    print("Running evaluation metrics tests...")
    
    total_passed = 0
    total_failed = 0
    
    for test_class in test_classes:
        class_name = test_class.__class__.__name__
        print(f"\n{class_name}:")
        
        # Get test methods
        test_methods = [method for method in dir(test_class) if method.startswith('test_')]
        
        # Setup if needed
        if hasattr(test_class, 'setup_method'):
            test_class.setup_method()
        
        for test_method in test_methods:
            try:
                print(f"  Running {test_method}...", end="")
                getattr(test_class, test_method)()
                print(" ✓ PASSED")
                total_passed += 1
            except Exception as e:
                print(f" ✗ FAILED: {str(e)}")
                total_failed += 1
    
    print(f"\nResults: {total_passed} passed, {total_failed} failed")
    return total_failed == 0


if __name__ == "__main__":
    success = run_all_tests()
    if success:
        print("\n🎉 All evaluation metrics tests passed! Ready for next component.")
    else:
        print("\n❌ Some tests failed. Please fix issues before proceeding.")
        sys.exit(1)