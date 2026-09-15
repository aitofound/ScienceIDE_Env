"""
Evaluation metrics for MRI reconstruction quality assessment
Includes standard medical imaging metrics like PSNR, SSIM, MSE, etc.
"""

import numpy as np
import torch
from typing import Union, Tuple, Optional
from scipy import ndimage
from skimage.metrics import structural_similarity as ssim_skimage
from skimage.metrics import peak_signal_noise_ratio as psnr_skimage


class ReconstructionMetrics:
    """
    Comprehensive evaluation metrics for MRI reconstruction quality
    """
    
    @staticmethod
    def mse(reference: np.ndarray, reconstruction: np.ndarray) -> float:
        """
        Mean Squared Error
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            
        Returns:
            MSE value
        """
        return np.mean((reference - reconstruction) ** 2)
    
    @staticmethod
    def rmse(reference: np.ndarray, reconstruction: np.ndarray) -> float:
        """
        Root Mean Squared Error
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            
        Returns:
            RMSE value
        """
        return np.sqrt(ReconstructionMetrics.mse(reference, reconstruction))
    
    @staticmethod
    def mae(reference: np.ndarray, reconstruction: np.ndarray) -> float:
        """
        Mean Absolute Error
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            
        Returns:
            MAE value
        """
        return np.mean(np.abs(reference - reconstruction))
    
    @staticmethod
    def psnr(reference: np.ndarray, reconstruction: np.ndarray, 
             data_range: Optional[float] = None) -> float:
        """
        Peak Signal-to-Noise Ratio
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            data_range: Data range of the images (max - min)
            
        Returns:
            PSNR in dB
        """
        if data_range is None:
            data_range = reference.max() - reference.min()
        
        return psnr_skimage(reference, reconstruction, data_range=data_range)
    
    @staticmethod
    def ssim(reference: np.ndarray, reconstruction: np.ndarray,
             data_range: Optional[float] = None,
             win_size: Optional[int] = None) -> float:
        """
        Structural Similarity Index Measure
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            data_range: Data range of the images
            win_size: Window size for SSIM calculation
            
        Returns:
            SSIM value between 0 and 1
        """
        if data_range is None:
            data_range = reference.max() - reference.min()
        
        # Handle small images by adjusting window size
        if win_size is None:
            min_dim = min(reference.shape)
            if min_dim < 7:
                win_size = min_dim if min_dim % 2 == 1 else min_dim - 1
                win_size = max(win_size, 3)  # Minimum window size of 3
        
        try:
            return ssim_skimage(reference, reconstruction, 
                              data_range=data_range, win_size=win_size)
        except (ValueError, RuntimeWarning):
            # Fallback for problematic cases (e.g., constant images)
            if np.allclose(reference, reconstruction):
                return 1.0
            else:
                # Return a reasonable fallback value
                return float('nan')
    
    @staticmethod
    def nrmse(reference: np.ndarray, reconstruction: np.ndarray,
              normalization: str = 'euclidean') -> float:
        """
        Normalized Root Mean Squared Error
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            normalization: Type of normalization ('euclidean', 'min-max', 'mean')
            
        Returns:
            Normalized RMSE value
        """
        rmse_val = ReconstructionMetrics.rmse(reference, reconstruction)
        
        if normalization == 'euclidean':
            norm_factor = np.sqrt(np.mean(reference ** 2))
        elif normalization == 'min-max':
            norm_factor = reference.max() - reference.min()
        elif normalization == 'mean':
            norm_factor = np.mean(reference)
        else:
            raise ValueError(f"Unknown normalization: {normalization}")
        
        return rmse_val / (norm_factor + 1e-8)
    
    @staticmethod
    def snr(reference: np.ndarray, reconstruction: np.ndarray) -> float:
        """
        Signal-to-Noise Ratio
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            
        Returns:
            SNR in dB
        """
        signal_power = np.mean(reference ** 2)
        noise_power = np.mean((reference - reconstruction) ** 2)
        
        if noise_power == 0:
            return float('inf')
        
        return 10 * np.log10(signal_power / noise_power)
    
    @staticmethod
    def correlation_coefficient(reference: np.ndarray, 
                              reconstruction: np.ndarray) -> float:
        """
        Pearson Correlation Coefficient
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            
        Returns:
            Correlation coefficient between -1 and 1
        """
        ref_flat = reference.flatten()
        rec_flat = reconstruction.flatten()
        
        return np.corrcoef(ref_flat, rec_flat)[0, 1]
    
    @staticmethod
    def edge_preservation_index(reference: np.ndarray, 
                              reconstruction: np.ndarray) -> float:
        """
        Edge Preservation Index (EPI)
        Measures how well edges are preserved in reconstruction
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            
        Returns:
            EPI value (higher is better)
        """
        # Compute gradients using Sobel filters
        def compute_gradients(img):
            grad_x = ndimage.sobel(img, axis=1)
            grad_y = ndimage.sobel(img, axis=0)
            return np.sqrt(grad_x**2 + grad_y**2)
        
        ref_edges = compute_gradients(reference)
        rec_edges = compute_gradients(reconstruction)
        
        # Compute correlation between edge maps
        ref_edges_flat = ref_edges.flatten()
        rec_edges_flat = rec_edges.flatten()
        
        # Add small epsilon to avoid division by zero
        numerator = np.sum(ref_edges_flat * rec_edges_flat)
        denominator = np.sqrt(np.sum(ref_edges_flat**2) * np.sum(rec_edges_flat**2))
        
        return numerator / (denominator + 1e-8)
    
    @staticmethod
    def blur_metric(image: np.ndarray) -> float:
        """
        Blur metric using Laplacian variance
        Lower values indicate more blur
        
        Args:
            image: Input image
            
        Returns:
            Blur metric (higher is sharper)
        """
        laplacian = ndimage.laplace(image)
        return np.var(laplacian)
    
    @staticmethod
    def artifact_power(reference: np.ndarray, reconstruction: np.ndarray,
                      background_threshold: float = 0.1) -> float:
        """
        Measure artifact power in background regions
        
        Args:
            reference: Ground truth image
            reconstruction: Reconstructed image
            background_threshold: Threshold to identify background pixels
            
        Returns:
            Artifact power (lower is better)
        """
        # Identify background regions in reference image
        ref_normalized = (reference - reference.min()) / (reference.max() - reference.min())
        background_mask = ref_normalized < background_threshold
        
        if not np.any(background_mask):
            return 0.0
        
        # Compute artifact power in background
        error = reconstruction - reference
        artifact_power = np.mean(error[background_mask] ** 2)
        
        return artifact_power


class BatchMetrics:
    """Compute metrics for batches of images"""
    
    @staticmethod
    def compute_batch_metrics(references: Union[np.ndarray, torch.Tensor],
                            reconstructions: Union[np.ndarray, torch.Tensor],
                            metrics: list = None) -> dict:
        """
        Compute metrics for a batch of images
        
        Args:
            references: Batch of reference images [N, H, W] or [N, C, H, W]
            reconstructions: Batch of reconstructed images
            metrics: List of metric names to compute
            
        Returns:
            Dictionary of metric values (mean, std, individual values)
        """
        if metrics is None:
            metrics = ['mse', 'psnr', 'ssim', 'nrmse', 'correlation']
        
        # Convert to numpy if needed
        if torch.is_tensor(references):
            references = references.detach().cpu().numpy()
        if torch.is_tensor(reconstructions):
            reconstructions = reconstructions.detach().cpu().numpy()
        
        # Handle channel dimension
        if references.ndim == 4 and references.shape[1] == 1:
            references = references.squeeze(1)
        if reconstructions.ndim == 4 and reconstructions.shape[1] == 1:
            reconstructions = reconstructions.squeeze(1)
        
        batch_size = references.shape[0]
        results = {metric: [] for metric in metrics}
        
        # Compute metrics for each image in batch
        for i in range(batch_size):
            ref_img = references[i]
            rec_img = reconstructions[i]
            
            for metric in metrics:
                if metric == 'mse':
                    value = ReconstructionMetrics.mse(ref_img, rec_img)
                elif metric == 'rmse':
                    value = ReconstructionMetrics.rmse(ref_img, rec_img)
                elif metric == 'mae':
                    value = ReconstructionMetrics.mae(ref_img, rec_img)
                elif metric == 'psnr':
                    value = ReconstructionMetrics.psnr(ref_img, rec_img)
                elif metric == 'ssim':
                    value = ReconstructionMetrics.ssim(ref_img, rec_img)
                elif metric == 'nrmse':
                    value = ReconstructionMetrics.nrmse(ref_img, rec_img)
                elif metric == 'snr':
                    value = ReconstructionMetrics.snr(ref_img, rec_img)
                elif metric == 'correlation':
                    value = ReconstructionMetrics.correlation_coefficient(ref_img, rec_img)
                elif metric == 'edge_preservation':
                    value = ReconstructionMetrics.edge_preservation_index(ref_img, rec_img)
                elif metric == 'blur':
                    value = ReconstructionMetrics.blur_metric(rec_img)
                elif metric == 'artifact_power':
                    value = ReconstructionMetrics.artifact_power(ref_img, rec_img)
                else:
                    raise ValueError(f"Unknown metric: {metric}")
                
                results[metric].append(value)
        
        # Compute statistics
        final_results = {}
        for metric, values in results.items():
            final_results[metric] = {
                'mean': np.mean(values),
                'std': np.std(values),
                'min': np.min(values),
                'max': np.max(values),
                'values': values
            }
        
        return final_results


def compare_reconstructions(reference: np.ndarray,
                          reconstructions: dict,
                          metrics: list = None) -> dict:
    """
    Compare multiple reconstruction methods
    
    Args:
        reference: Ground truth image
        reconstructions: Dict of {method_name: reconstructed_image}
        metrics: List of metrics to compute
        
    Returns:
        Dictionary of results for each method
    """
    if metrics is None:
        metrics = ['mse', 'psnr', 'ssim', 'nrmse', 'correlation', 'edge_preservation']
    
    results = {}
    
    for method_name, reconstruction in reconstructions.items():
        method_results = {}
        
        for metric in metrics:
            if metric == 'mse':
                value = ReconstructionMetrics.mse(reference, reconstruction)
            elif metric == 'rmse':
                value = ReconstructionMetrics.rmse(reference, reconstruction)
            elif metric == 'mae':
                value = ReconstructionMetrics.mae(reference, reconstruction)
            elif metric == 'psnr':
                value = ReconstructionMetrics.psnr(reference, reconstruction)
            elif metric == 'ssim':
                value = ReconstructionMetrics.ssim(reference, reconstruction)
            elif metric == 'nrmse':
                value = ReconstructionMetrics.nrmse(reference, reconstruction)
            elif metric == 'snr':
                value = ReconstructionMetrics.snr(reference, reconstruction)
            elif metric == 'correlation':
                value = ReconstructionMetrics.correlation_coefficient(reference, reconstruction)
            elif metric == 'edge_preservation':
                value = ReconstructionMetrics.edge_preservation_index(reference, reconstruction)
            elif metric == 'blur':
                value = ReconstructionMetrics.blur_metric(reconstruction)
            elif metric == 'artifact_power':
                value = ReconstructionMetrics.artifact_power(reference, reconstruction)
            else:
                raise ValueError(f"Unknown metric: {metric}")
            
            method_results[metric] = value
        
        results[method_name] = method_results
    
    return results


def main():
    """Test the evaluation metrics"""
    print("Testing MRI reconstruction evaluation metrics...")
    
    # Create test images
    np.random.seed(42)
    reference = np.random.rand(128, 128)
    
    # Create different quality reconstructions
    perfect = reference.copy()
    noisy = reference + 0.1 * np.random.randn(*reference.shape)
    blurry = ndimage.gaussian_filter(reference, sigma=1.0)
    
    reconstructions = {
        'Perfect': perfect,
        'Noisy': noisy,
        'Blurry': blurry
    }
    
    # Compare reconstructions
    results = compare_reconstructions(reference, reconstructions)
    
    print("\nReconstruction Quality Comparison:")
    print("=" * 60)
    
    for method, metrics in results.items():
        print(f"\n{method}:")
        for metric, value in metrics.items():
            if metric in ['psnr', 'snr']:
                print(f"  {metric.upper()}: {value:.2f} dB")
            elif metric in ['ssim', 'correlation', 'edge_preservation']:
                print(f"  {metric.upper()}: {value:.4f}")
            else:
                print(f"  {metric.upper()}: {value:.6f}")
    
    # Test batch metrics
    print(f"\nTesting batch metrics...")
    batch_refs = np.random.rand(5, 64, 64)
    batch_recs = batch_refs + 0.05 * np.random.randn(5, 64, 64)
    
    batch_results = BatchMetrics.compute_batch_metrics(batch_refs, batch_recs)
    
    print("Batch Results (5 images):")
    for metric, stats in batch_results.items():
        print(f"  {metric.upper()}: {stats['mean']:.4f} ± {stats['std']:.4f}")
    
    print("\nEvaluation metrics working correctly!")


if __name__ == "__main__":
    main()