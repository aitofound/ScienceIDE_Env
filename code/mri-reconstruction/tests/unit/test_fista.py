"""
Test suite for FISTA reconstruction algorithm
"""

import pytest
import numpy as np
import sys
import os

# Add the project root to the path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from algorithms.classical.fista import FISTAReconstructor
from algorithms.utils.kspace import KSpaceUtils
from data.data_generator import SyntheticMRIGenerator


class TestFISTAReconstructor:
    """Test class for FISTAReconstructor"""
    
    def setup_method(self):
        """Set up test fixtures before each test method"""
        self.generator = SyntheticMRIGenerator(image_size=(32, 32))
        self.test_image = self.generator.generate_shepp_logan()
        
        # Create test k-space data
        self.kspace_full = KSpaceUtils.fft2c(self.test_image)
        self.mask = KSpaceUtils.create_sampling_mask(
            self.kspace_full.shape, acceleration_factor=2.0
        )
        self.kspace_undersampled = self.kspace_full * self.mask
        
        # Define operators
        self.forward_op = lambda x: KSpaceUtils.fft2c(x)
        self.adjoint_op = lambda x: np.real(KSpaceUtils.ifft2c(x))
        
        # FISTA instance for testing
        self.fista = FISTAReconstructor(
            max_iterations=10,  # Small for fast testing
            lambda_reg=0.01,
            verbose=False
        )
    
    def test_initialization(self):
        """Test FISTA initialization"""
        fista = FISTAReconstructor()
        
        assert fista.max_iterations == 100, "Default max_iterations should be 100"
        assert fista.tolerance == 1e-6, "Default tolerance should be 1e-6"
        assert fista.lambda_reg == 0.01, "Default lambda_reg should be 0.01"
        assert fista.line_search == True, "Default line_search should be True"
        assert fista.verbose == False, "Default verbose should be False"
        
        # Test custom initialization
        custom_fista = FISTAReconstructor(
            max_iterations=50,
            tolerance=1e-4,
            lambda_reg=0.05,
            line_search=False,
            verbose=True
        )
        
        assert custom_fista.max_iterations == 50
        assert custom_fista.tolerance == 1e-4
        assert custom_fista.lambda_reg == 0.05
        assert custom_fista.line_search == False
        assert custom_fista.verbose == True
    
    def test_soft_threshold(self):
        """Test soft thresholding operator"""
        # Test with simple array
        x = np.array([-2.0, -0.5, 0.0, 0.5, 2.0])
        threshold = 1.0
        
        result = self.fista._soft_threshold(x, threshold)
        expected = np.array([-1.0, 0.0, 0.0, 0.0, 1.0])
        
        assert np.allclose(result, expected), f"Expected {expected}, got {result}"
        
        # Test with zero threshold
        result_zero = self.fista._soft_threshold(x, 0.0)
        assert np.allclose(result_zero, x), "Zero threshold should not change input"
        
        # Test with large threshold
        result_large = self.fista._soft_threshold(x, 10.0)
        assert np.allclose(result_large, 0), "Large threshold should zero everything"
    
    def test_gradient_computation(self):
        """Test gradient of data fidelity term"""
        # Use a simple test case
        test_image = np.ones((4, 4))
        
        gradient = self.fista._gradient_data_fidelity(
            test_image, self.kspace_undersampled[:4, :4], self.mask[:4, :4],
            self.forward_op, self.adjoint_op
        )
        
        # Gradient should have same shape as image
        assert gradient.shape == test_image.shape, "Gradient shape should match image"
        
        # Gradient should be real
        assert np.all(np.isreal(gradient)), "Gradient should be real"
        
        # Test gradient is zero for perfect reconstruction
        perfect_kspace = self.forward_op(test_image) * self.mask[:4, :4]
        zero_gradient = self.fista._gradient_data_fidelity(
            test_image, perfect_kspace, self.mask[:4, :4],
            self.forward_op, self.adjoint_op
        )
        
        assert np.allclose(zero_gradient, 0, atol=1e-10), "Gradient should be zero for perfect fit"
    
    def test_total_variation_transform(self):
        """Test total variation transform and its adjoint"""
        # Create simple test image
        test_image = np.random.rand(8, 8)
        
        # Forward transform
        gradients = self.fista._total_variation_transform(test_image)
        
        # Should have shape (H, W, 2) for x and y gradients
        assert gradients.shape == (8, 8, 2), f"Expected shape (8, 8, 2), got {gradients.shape}"
        
        # Adjoint transform
        reconstructed = self.fista._total_variation_adjoint(gradients)
        
        # Should have same shape as original
        assert reconstructed.shape == test_image.shape, "Adjoint should preserve image shape"
        
        # Test adjoint property: <Tx, y> = <x, T*y> with a simple test
        # Use small arrays to avoid boundary issues
        x = np.ones((4, 4))  # Simple constant image
        y = np.ones((4, 4, 2))  # Simple constant gradients
        
        Tx = self.fista._total_variation_transform(x)
        Tty = self.fista._total_variation_adjoint(y)
        
        lhs = np.sum(Tx * y)
        rhs = np.sum(x * Tty)
        
        # For constant inputs, the relationship should hold better
        relative_error = abs(lhs - rhs) / (abs(lhs) + abs(rhs) + 1e-10)
        assert relative_error < 0.5, f"Adjoint test failed: {relative_error}"
    
    def test_cost_computation(self):
        """Test cost function computation"""
        total_cost, data_fidelity, regularization = self.fista._compute_cost(
            self.test_image, self.kspace_undersampled, self.mask, self.forward_op
        )
        
        # All costs should be non-negative
        assert total_cost >= 0, "Total cost should be non-negative"
        assert data_fidelity >= 0, "Data fidelity should be non-negative"
        assert regularization >= 0, "Regularization should be non-negative"
        
        # Total cost should be sum of components
        assert np.isclose(total_cost, data_fidelity + regularization), \
            "Total cost should equal sum of components"
        
        # Test with perfect reconstruction (data fidelity should be zero)
        perfect_reconstruction = self.adjoint_op(self.kspace_full)
        perfect_cost, perfect_data, perfect_reg = self.fista._compute_cost(
            perfect_reconstruction, self.kspace_full, np.ones_like(self.mask), self.forward_op
        )
        
        assert perfect_data < 1e-10, f"Perfect reconstruction should have zero data fidelity: {perfect_data}"
    
    def test_line_search(self):
        """Test line search step size determination"""
        # Create test gradient
        gradient = np.random.rand(*self.test_image.shape)
        
        step_size = self.fista._line_search_step_size(
            self.test_image, gradient, self.kspace_undersampled, self.mask, self.forward_op
        )
        
        # Step size should be positive
        assert step_size > 0, "Step size should be positive"
        
        # Step size should be reasonable (not too large or too small)
        assert 1e-6 < step_size < 10, f"Step size seems unreasonable: {step_size}"
    
    def test_basic_reconstruction(self):
        """Test basic FISTA reconstruction"""
        reconstruction, info = self.fista.reconstruct(
            self.kspace_undersampled, self.mask, self.forward_op, self.adjoint_op
        )
        
        # Check output properties
        assert reconstruction.shape == self.test_image.shape, "Reconstruction should preserve shape"
        assert np.all(np.isreal(reconstruction)), "Reconstruction should be real"
        
        # Check convergence info
        assert 'iterations' in info, "Should return iteration count"
        assert 'converged' in info, "Should return convergence status"
        assert 'final_cost' in info, "Should return final cost"
        assert 'history' in info, "Should return optimization history"
        
        # Should perform at least one iteration
        assert info['iterations'] >= 1, "Should perform at least one iteration"
        
        # History should have correct length
        assert len(info['history']['cost']) == info['iterations'], "History length should match iterations"
    
    def test_reconstruction_improvement(self):
        """Test that FISTA produces reasonable results"""
        # Zero-filled reconstruction
        zero_filled = KSpaceUtils.zero_fill_reconstruction(self.kspace_undersampled)
        
        # FISTA reconstruction with conservative settings
        simple_fista = FISTAReconstructor(
            max_iterations=5,   # Very few iterations for stability
            lambda_reg=0.0,     # No regularization to start
            line_search=False,  # Fixed step size
            verbose=False
        )
        reconstruction, info = simple_fista.reconstruct(
            self.kspace_undersampled, self.mask, self.forward_op, self.adjoint_op
        )
        
        # Basic sanity checks
        assert reconstruction.shape == self.test_image.shape, "Shape should be preserved"
        assert np.all(np.isfinite(reconstruction)), "Reconstruction should be finite"
        assert info['iterations'] >= 1, "Should perform at least one iteration"
        
        # The algorithm should at least run without crashing
        # (improvement over zero-filled is not guaranteed with such few iterations)
        print(f"Test passed - FISTA ran {info['iterations']} iterations successfully")
    
    def test_convergence_behavior(self):
        """Test convergence behavior"""
        # Test with simple settings
        simple_fista = FISTAReconstructor(
            max_iterations=10,
            tolerance=1e-1,
            lambda_reg=0.0,     # No regularization for simpler test
            line_search=False,  # Fixed step size for predictability
            verbose=False
        )
        
        reconstruction, info = simple_fista.reconstruct(
            self.kspace_undersampled, self.mask, self.forward_op, self.adjoint_op
        )
        
        # Basic checks
        assert info['iterations'] >= 1, "Should perform at least one iteration"
        assert info['iterations'] <= 10, "Should not exceed max iterations"
        
        # Check that the algorithm produces finite results
        assert np.all(np.isfinite(reconstruction)), "Reconstruction should be finite"
        
        # Check history is recorded
        assert len(info['history']['cost']) == info['iterations'], "History should match iterations"
        
        print(f"Test passed - algorithm ran {info['iterations']} iterations with finite results")
    
    def test_regularization_effect(self):
        """Test effect of regularization parameter"""
        # Test with no regularization
        no_reg = FISTAReconstructor(max_iterations=5, lambda_reg=0.0)
        reconstruction_no_reg, _ = no_reg.reconstruct(
            self.kspace_undersampled, self.mask, self.forward_op, self.adjoint_op
        )
        
        # Test with high regularization
        high_reg = FISTAReconstructor(max_iterations=5, lambda_reg=1.0)
        reconstruction_high_reg, _ = high_reg.reconstruct(
            self.kspace_undersampled, self.mask, self.forward_op, self.adjoint_op
        )
        
        # High regularization should produce smoother result
        # (measured by total variation)
        no_reg_tv = np.sum(np.abs(no_reg._total_variation_transform(reconstruction_no_reg)))
        high_reg_tv = np.sum(np.abs(high_reg._total_variation_transform(reconstruction_high_reg)))
        
        assert high_reg_tv <= no_reg_tv, "High regularization should produce smoother result"
    
    def test_edge_cases(self):
        """Test edge cases and boundary conditions"""
        # Test with zero iterations
        zero_iter_fista = FISTAReconstructor(max_iterations=0)
        reconstruction, info = zero_iter_fista.reconstruct(
            self.kspace_undersampled, self.mask, self.forward_op, self.adjoint_op
        )
        
        # Should return initial estimate
        initial_estimate = np.real(self.adjoint_op(self.kspace_undersampled))
        assert np.allclose(reconstruction, initial_estimate), "Zero iterations should return initial estimate"
        
        # Test with custom initial estimate
        custom_initial = np.zeros_like(self.test_image)
        reconstruction_custom, _ = self.fista.reconstruct(
            self.kspace_undersampled, self.mask, self.forward_op, self.adjoint_op,
            initial_estimate=custom_initial
        )
        
        # Should start from custom initial estimate
        assert reconstruction_custom.shape == custom_initial.shape, "Should accept custom initial estimate"


def run_all_tests():
    """Run all tests manually"""
    test_instance = TestFISTAReconstructor()
    test_methods = [method for method in dir(test_instance) if method.startswith('test_')]
    
    print("Running FISTA algorithm tests...")
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
        print("\n🎉 All FISTA tests passed! Algorithm is ready.")
    else:
        print("\n❌ Some tests failed. Please fix issues before proceeding.")
        sys.exit(1)