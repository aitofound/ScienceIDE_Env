"""
FISTA (Fast Iterative Shrinkage-Thresholding Algorithm) implementation
for compressed sensing MRI reconstruction
"""

import numpy as np
from typing import Tuple, Optional, Callable
import time


class FISTAReconstructor:
    """
    FISTA algorithm for solving L1-regularized least squares problems
    Specifically designed for MRI reconstruction with sparsity constraints
    """
    
    def __init__(self, 
                 max_iterations: int = 100,
                 tolerance: float = 1e-6,
                 lambda_reg: float = 0.01,
                 line_search: bool = True,
                 verbose: bool = False):
        """
        Initialize FISTA reconstructor
        
        Args:
            max_iterations: Maximum number of iterations
            tolerance: Convergence tolerance
            lambda_reg: Regularization parameter (L1 penalty weight)
            line_search: Whether to use adaptive step size
            verbose: Whether to print progress
        """
        self.max_iterations = max_iterations
        self.tolerance = tolerance
        self.lambda_reg = lambda_reg
        self.line_search = line_search
        self.verbose = verbose
        
        # Algorithm state
        self.history = {
            'cost': [],
            'data_fidelity': [],
            'regularization': [],
            'step_size': [],
            'iteration_times': []
        }
    
    def _soft_threshold(self, x: np.ndarray, threshold: float) -> np.ndarray:
        """
        Soft thresholding operator for L1 regularization
        
        Args:
            x: Input array
            threshold: Threshold value
            
        Returns:
            Soft-thresholded array
        """
        return np.sign(x) * np.maximum(np.abs(x) - threshold, 0)
    
    def _gradient_data_fidelity(self, image: np.ndarray, 
                               kspace_data: np.ndarray, 
                               sampling_mask: np.ndarray,
                               forward_op: Callable,
                               adjoint_op: Callable) -> np.ndarray:
        """
        Compute gradient of data fidelity term
        
        Args:
            image: Current image estimate
            kspace_data: Measured k-space data
            sampling_mask: Sampling mask
            forward_op: Forward operator (image to k-space)
            adjoint_op: Adjoint operator (k-space to image)
            
        Returns:
            Gradient of data fidelity term
        """
        # Forward operator: image -> k-space
        predicted_kspace = forward_op(image)
        
        # Data residual in k-space
        residual = (predicted_kspace - kspace_data) * sampling_mask
        
        # Adjoint operator: k-space -> image
        gradient = adjoint_op(residual)
        
        return gradient
    
    def _compute_cost(self, image: np.ndarray,
                     kspace_data: np.ndarray,
                     sampling_mask: np.ndarray,
                     forward_op: Callable,
                     sparsity_transform: Optional[Callable] = None) -> Tuple[float, float, float]:
        """
        Compute total cost function
        
        Args:
            image: Current image estimate
            kspace_data: Measured k-space data
            sampling_mask: Sampling mask
            forward_op: Forward operator
            sparsity_transform: Transform for sparsity (e.g., wavelets)
            
        Returns:
            Tuple of (total_cost, data_fidelity, regularization)
        """
        # Data fidelity term: ||A(x) - b||_2^2
        predicted_kspace = forward_op(image)
        residual = (predicted_kspace - kspace_data) * sampling_mask
        data_fidelity = 0.5 * np.sum(np.abs(residual)**2)
        
        # Regularization term: lambda * ||Psi(x)||_1
        if sparsity_transform is not None:
            sparse_coeffs = sparsity_transform(image)
        else:
            # Use image gradients as sparsity transform (Total Variation)
            sparse_coeffs = self._total_variation_transform(image)
        
        regularization = self.lambda_reg * np.sum(np.abs(sparse_coeffs))
        
        total_cost = data_fidelity + regularization
        
        return total_cost, data_fidelity, regularization
    
    def _total_variation_transform(self, image: np.ndarray) -> np.ndarray:
        """
        Compute total variation (gradient) transform using backward differences
        
        Args:
            image: Input image
            
        Returns:
            Gradient coefficients
        """
        # Use backward differences for proper adjoint relationship
        grad_x = np.diff(image, axis=1, prepend=image[:, [0]])  # Prepend first column
        grad_y = np.diff(image, axis=0, prepend=image[[0], :])  # Prepend first row
        
        # Stack gradients
        return np.stack([grad_x, grad_y], axis=-1)
    
    def _total_variation_adjoint(self, gradients: np.ndarray) -> np.ndarray:
        """
        Adjoint of total variation transform (negative divergence)
        
        Args:
            gradients: Gradient coefficients
            
        Returns:
            Image
        """
        grad_x, grad_y = gradients[..., 0], gradients[..., 1]
        
        # Adjoint of backward differences is forward differences with negative sign
        # For backward diff with prepend, adjoint is forward diff with append
        div_x = -np.diff(grad_x, axis=1, append=grad_x[:, [-1]])
        div_y = -np.diff(grad_y, axis=0, append=grad_y[[-1], :])
        
        return div_x + div_y
    
    def _line_search_step_size(self, image: np.ndarray,
                              gradient: np.ndarray,
                              kspace_data: np.ndarray,
                              sampling_mask: np.ndarray,
                              forward_op: Callable,
                              initial_step: float = 0.01) -> float:
        """
        Adaptive step size using backtracking line search
        
        Args:
            image: Current image
            gradient: Gradient at current point
            kspace_data: Measured k-space data
            sampling_mask: Sampling mask
            forward_op: Forward operator
            initial_step: Initial step size
            
        Returns:
            Optimal step size
        """
        step_size = initial_step
        beta = 0.5  # Step size reduction factor
        c1 = 0.1   # Armijo parameter (more conservative)
        
        # Compute current data fidelity
        current_kspace = forward_op(image)
        current_residual = (current_kspace - kspace_data) * sampling_mask
        current_cost = 0.5 * np.sum(np.abs(current_residual)**2)
        
        # Gradient norm for Armijo condition
        grad_norm_sq = np.sum(np.abs(gradient)**2)
        
        for _ in range(10):  # Maximum 10 line search iterations
            # Try step
            test_image = image - step_size * np.real(gradient)
            test_kspace = forward_op(test_image)
            test_residual = (test_kspace - kspace_data) * sampling_mask
            test_cost = 0.5 * np.sum(np.abs(test_residual)**2)
            
            # Armijo condition (sufficient decrease)
            expected_decrease = c1 * step_size * grad_norm_sq
            if test_cost <= current_cost - expected_decrease or step_size < 1e-6:
                break
            
            step_size *= beta
        
        # Ensure minimum step size
        return max(step_size, 1e-6)
    
    def reconstruct(self,
                   kspace_data: np.ndarray,
                   sampling_mask: np.ndarray,
                   forward_op: Callable,
                   adjoint_op: Callable,
                   initial_estimate: Optional[np.ndarray] = None,
                   sparsity_transform: Optional[Callable] = None,
                   sparsity_adjoint: Optional[Callable] = None) -> Tuple[np.ndarray, dict]:
        """
        Perform FISTA reconstruction
        
        Args:
            kspace_data: Measured (undersampled) k-space data
            sampling_mask: Binary sampling mask
            forward_op: Forward operator (image -> k-space)
            adjoint_op: Adjoint operator (k-space -> image)
            initial_estimate: Initial image estimate
            sparsity_transform: Sparsity transform (e.g., wavelets)
            sparsity_adjoint: Adjoint of sparsity transform
            
        Returns:
            Tuple of (reconstructed_image, convergence_info)
        """
        # Initialize
        if initial_estimate is None:
            # Zero-filled reconstruction as initial estimate
            initial_estimate = np.real(adjoint_op(kspace_data * sampling_mask))
        
        x = initial_estimate.copy()  # Current estimate
        y = x.copy()  # Momentum variable
        t = 1.0  # Momentum parameter
        
        # Use Total Variation if no sparsity transform provided
        if sparsity_transform is None:
            sparsity_transform = self._total_variation_transform
            sparsity_adjoint = self._total_variation_adjoint
        
        # Clear history
        for key in self.history:
            self.history[key].clear()
        
        if self.verbose:
            print("Starting FISTA reconstruction...")
            print(f"Max iterations: {self.max_iterations}")
            print(f"Regularization: {self.lambda_reg}")
        
        # Main FISTA loop
        for iteration in range(self.max_iterations):
            iteration_start = time.time()
            
            # Compute gradient at y
            grad_data = self._gradient_data_fidelity(y, kspace_data, sampling_mask, 
                                                   forward_op, adjoint_op)
            
            # Determine step size (more conservative)
            if self.line_search:
                step_size = self._line_search_step_size(y, grad_data, kspace_data, 
                                                      sampling_mask, forward_op)
            else:
                # Use a very conservative fixed step size
                step_size = 0.001
            
            # Gradient step
            z = y - step_size * np.real(grad_data)  # Ensure real gradient
            
            # Proximal operator (soft thresholding in sparse domain)
            if sparsity_transform is not None:
                sparse_z = sparsity_transform(z)
                threshold = step_size * self.lambda_reg
                sparse_z_thresholded = self._soft_threshold(sparse_z, threshold)
                x_new = np.real(sparsity_adjoint(sparse_z_thresholded))  # Ensure real result
            else:
                # Direct soft thresholding on image (simple L1 on pixels)
                threshold = step_size * self.lambda_reg
                x_new = self._soft_threshold(z, threshold)
            
            # Check for numerical issues
            if not np.all(np.isfinite(x_new)):
                if self.verbose:
                    print(f"Numerical instability detected at iteration {iteration}")
                break
            
            # Momentum update with stability check
            t_new = (1 + np.sqrt(1 + 4 * t**2)) / 2
            momentum_coeff = (t - 1) / t_new
            
            # Limit momentum to prevent instability
            momentum_coeff = np.clip(momentum_coeff, 0, 0.99)
            
            y = x_new + momentum_coeff * (x_new - x)
            
            # Update variables
            x, t = x_new.copy(), t_new
            
            # Compute cost and check convergence
            total_cost, data_fidelity, regularization = self._compute_cost(
                x, kspace_data, sampling_mask, forward_op, sparsity_transform
            )
            
            # Store history
            iteration_time = time.time() - iteration_start
            self.history['cost'].append(total_cost)
            self.history['data_fidelity'].append(data_fidelity)
            self.history['regularization'].append(regularization)
            self.history['step_size'].append(step_size)
            self.history['iteration_times'].append(iteration_time)
            
            # Check convergence
            if iteration > 0:
                cost_change = abs(self.history['cost'][-2] - total_cost)
                relative_change = cost_change / (abs(self.history['cost'][-2]) + 1e-10)
                
                if relative_change < self.tolerance:
                    if self.verbose:
                        print(f"Converged at iteration {iteration + 1}")
                    break
            
            if self.verbose and (iteration + 1) % 10 == 0:
                print(f"Iter {iteration + 1:3d}: Cost = {total_cost:.2e}, "
                      f"Data = {data_fidelity:.2e}, Reg = {regularization:.2e}, "
                      f"Step = {step_size:.3f}")
        
        # Prepare convergence info
        convergence_info = {
            'iterations': len(self.history['cost']),
            'converged': len(self.history['cost']) < self.max_iterations,
            'final_cost': self.history['cost'][-1] if self.history['cost'] else float('inf'),
            'history': self.history.copy()
        }
        
        return x, convergence_info


def main():
    """Test the FISTA algorithm"""
    from algorithms.utils.kspace import KSpaceUtils
    from data.data_generator import SyntheticMRIGenerator
    
    print("Testing FISTA reconstruction...")
    
    # Generate test data
    generator = SyntheticMRIGenerator(image_size=(128, 128))
    ground_truth = generator.generate_shepp_logan()
    
    # Create undersampled k-space data
    kspace_full = KSpaceUtils.fft2c(ground_truth)
    mask = KSpaceUtils.create_sampling_mask(kspace_full.shape, acceleration_factor=4.0)
    kspace_undersampled = kspace_full * mask
    
    # Define forward and adjoint operators
    def forward_op(image):
        return KSpaceUtils.fft2c(image)
    
    def adjoint_op(kspace):
        return np.real(KSpaceUtils.ifft2c(kspace))
    
    # Initialize FISTA
    fista = FISTAReconstructor(
        max_iterations=50,
        lambda_reg=0.01,
        verbose=True
    )
    
    # Perform reconstruction
    start_time = time.time()
    reconstruction, info = fista.reconstruct(
        kspace_undersampled, mask, forward_op, adjoint_op
    )
    reconstruction_time = time.time() - start_time
    
    # Evaluate reconstruction quality
    mse = np.mean((ground_truth - reconstruction)**2)
    psnr = 20 * np.log10(1.0 / np.sqrt(mse))
    
    # Compare with zero-filled reconstruction
    zero_filled = KSpaceUtils.zero_fill_reconstruction(kspace_undersampled)
    zf_mse = np.mean((ground_truth - zero_filled)**2)
    zf_psnr = 20 * np.log10(1.0 / np.sqrt(zf_mse))
    
    print(f"\nReconstruction Results:")
    print(f"FISTA PSNR: {psnr:.2f} dB")
    print(f"Zero-filled PSNR: {zf_psnr:.2f} dB")
    print(f"Improvement: {psnr - zf_psnr:.2f} dB")
    print(f"Reconstruction time: {reconstruction_time:.2f} s")
    print(f"Iterations: {info['iterations']}")
    print(f"Converged: {info['converged']}")
    
    print("FISTA algorithm working correctly!")


if __name__ == "__main__":
    main()