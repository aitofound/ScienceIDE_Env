"""
MRI Reconstruction Pipeline - Main Reconstructor
Coordinates the complete reconstruction workflow
"""

# Setup project paths first
import sys
from pathlib import Path

# Add project root to path
current_file = Path(__file__).resolve()
project_root = current_file.parent.parent.parent  # Go up from pipeline/reconstruction/
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

# Alternative approach using the utility (once you create it):
# from utils.path_setup import setup_project_paths
# setup_project_paths()

import numpy as np
import torch
import time
from typing import Dict, List, Optional, Tuple

# Import our components
from data.data_generator import SyntheticMRIGenerator
from algorithms.utils.kspace import KSpaceUtils
from algorithms.classical.fista import FISTAReconstructor
from algorithms.ai.unet import UNet
from evaluation.metrics.reconstruction_metrics import compare_reconstructions


class MRIReconstructionPipeline:
    """
    Complete modular pipeline for MRI reconstruction experiments
    Uses dedicated preprocessing, reconstruction, and postprocessing modules
    """
    
    def __init__(self, image_size: Tuple[int, int] = (256, 256)):
        """
        Initialize the reconstruction pipeline
        
        Args:
            image_size: Size of MRI images to process
        """
        self.image_size = image_size
        
        # Initialize modules
        self.data_generator = SyntheticMRIGenerator(image_size=image_size)
        
        # For now, we'll use the preprocessing and postprocessing methods directly
        # Later you can uncomment these when the separate modules are set up:
        # self.preprocessor = MRIPreprocessor()
        # self.postprocessor = MRIPostprocessor()
        
        # Initialize reconstruction algorithms
        # Initialize reconstruction algorithms
        self.fista = FISTAReconstructor(
         max_iterations=20,     # Reduce iterations for stability
            lambda_reg=0.001,      # Much smaller regularization
             tolerance=1e-4,        # Looser tolerance
         line_search=False,     # Use fixed step size for stability
         verbose=False
            )
        
        # Setup U-Net (simplified - would load pre-trained in practice)
        self.unet = UNet(n_channels=1, n_classes=1)
        self.unet.eval()
        
        # Results storage
        self.experiment_results = {}
    
    def normalize_image(self, image: np.ndarray) -> Tuple[np.ndarray, Dict]:
        """Simple image normalization"""
        min_val = image.min()
        max_val = image.max()
        normalized = (image - min_val) / (max_val - min_val + 1e-8)
        params = {'min': min_val, 'max': max_val}
        return normalized, params
    
    def prepare_kspace_data(self, image: np.ndarray, acceleration_factor: float) -> Dict:
        """Prepare k-space data with undersampling"""
        # Convert to k-space
        kspace_full = KSpaceUtils.fft2c(image)
        
        # Create sampling mask
        mask = KSpaceUtils.create_sampling_mask(
            kspace_full.shape,
            acceleration_factor=acceleration_factor,
            center_fraction=0.08,
            pattern='1d_random'
        )
        
        # Apply undersampling
        kspace_undersampled = KSpaceUtils.apply_sampling_mask(kspace_full, mask)
        
        # Add noise
        kspace_undersampled = KSpaceUtils.add_noise_to_kspace(kspace_undersampled, snr_db=30.0)
        
        # Zero-filled reconstruction
        zero_filled = KSpaceUtils.zero_fill_reconstruction(kspace_undersampled)
        
        return {
            'kspace_full': kspace_full,
            'kspace_undersampled': kspace_undersampled,
            'sampling_mask': mask,
            'zero_filled': zero_filled,
            'sampling_percentage': KSpaceUtils.calculate_sampling_percentage(mask),
            'acceleration_factor': acceleration_factor
        }
    
    def generate_experiment_data(self, 
                               num_phantoms: int = 5,
                               acceleration_factors: List[float] = [2.0, 4.0, 6.0]) -> Dict:
        """
        Generate comprehensive experiment dataset
        
        Args:
            num_phantoms: Number of different phantom images
            acceleration_factors: List of undersampling factors
            
        Returns:
            Complete dataset dictionary
        """
        print(f"Generating experiment dataset...")
        print(f"  Phantoms: {num_phantoms}")
        print(f"  Acceleration factors: {acceleration_factors}")
        
        dataset = {
            'ground_truth_images': [],
            'preprocessed_data': {},
            'metadata': {
                'num_phantoms': num_phantoms,
                'acceleration_factors': acceleration_factors,
                'image_size': self.image_size
            }
        }
        
        # Generate ground truth phantoms
        for i in range(num_phantoms):
            if i == 0:
                # Standard Shepp-Logan phantom
                phantom = self.data_generator.generate_shepp_logan()
            else:
                # Brain-like phantoms with varying complexity
                phantom = self.data_generator.generate_brain_like(num_structures=3+i)
            
            # Normalize phantom
            normalized_phantom, norm_params = self.normalize_image(phantom)
            
            dataset['ground_truth_images'].append({
                'image': normalized_phantom,
                'normalization_params': norm_params,
                'phantom_type': 'shepp_logan' if i == 0 else f'brain_like_{i}'
            })
        
        # Generate undersampled data for each acceleration factor
        for acc_factor in acceleration_factors:
            dataset['preprocessed_data'][acc_factor] = []
            
            for phantom_data in dataset['ground_truth_images']:
                # Prepare k-space data with undersampling
                kspace_data = self.prepare_kspace_data(
                    phantom_data['image'],
                    acceleration_factor=acc_factor
                )
                
                dataset['preprocessed_data'][acc_factor].append({
                    'phantom_info': phantom_data,
                    'kspace_data': kspace_data
                })
        
        print("Dataset generation complete!")
        return dataset
    
    def reconstruct_with_method(self, 
                              method: str,
                              kspace_data: Dict,
                              ground_truth: np.ndarray) -> Tuple[np.ndarray, Dict]:
        """
        Perform reconstruction with specified method
        
        Args:
            method: Reconstruction method ('zero_filled', 'fista', 'unet')
            kspace_data: Preprocessed k-space data
            ground_truth: Ground truth image for reference
            
        Returns:
            Tuple of (reconstructed_image, method_info)
        """
        start_time = time.time()
        
        if method == 'zero_filled':
            reconstruction = kspace_data['zero_filled']
            method_info = {
                'method': 'zero_filled',
                'reconstruction_time': time.time() - start_time,
                'parameters': {},
                'convergence_info': None
            }
            
        elif method == 'fista':
            # Define operators
            def forward_op(image):
                return KSpaceUtils.fft2c(image)
            
            def adjoint_op(kspace):
                return np.real(KSpaceUtils.ifft2c(kspace))
            
            # Use zero-filled as initial estimate for better stability
            initial_estimate = kspace_data['zero_filled']
            
            # Perform FISTA reconstruction
            reconstruction, convergence_info = self.fista.reconstruct(
                kspace_data['kspace_undersampled'],
                kspace_data['sampling_mask'],
                forward_op,
                adjoint_op,
                initial_estimate=initial_estimate  # Add this line
            )
            
            # Ensure reconstruction is in valid range
            reconstruction = np.clip(reconstruction, 0, 1)  # Add this line
            
            method_info = {
                'method': 'fista',
                'reconstruction_time': time.time() - start_time,
                'parameters': {
                    'max_iterations': self.fista.max_iterations,
                    'lambda_reg': self.fista.lambda_reg,
                    'tolerance': self.fista.tolerance
                },
                'convergence_info': convergence_info
            }
            
        elif method == 'unet':
            # Use zero-filled as input to U-Net
            input_image = kspace_data['zero_filled']
            
            # Convert to tensor and reconstruct
            input_tensor = torch.from_numpy(input_image[np.newaxis, np.newaxis, ...]).float()
            
            with torch.no_grad():
                output_tensor = self.unet(input_tensor)
            
            reconstruction = output_tensor.squeeze().numpy()
            
            method_info = {
                'method': 'unet',
                'reconstruction_time': time.time() - start_time,
                'parameters': {
                    'model_parameters': self.unet.count_parameters(),
                    'input_channels': self.unet.n_channels,
                    'output_channels': self.unet.n_classes
                },
                'convergence_info': None
            }
            
        else:
            raise ValueError(f"Unknown reconstruction method: {method}")
        
        return reconstruction, method_info
    
    def run_comprehensive_experiment(self, 
                                   dataset: Dict,
                                   methods: List[str] = None,
                                   save_results: bool = True,
                                   output_dir: str = "results") -> Dict:
        """
        Run comprehensive reconstruction experiment
        
        Args:
            dataset: Dataset from generate_experiment_data()
            methods: List of methods to evaluate
            save_results: Whether to save detailed results
            output_dir: Output directory for results
            
        Returns:
            Complete experiment results
        """
        if methods is None:
            methods = ['zero_filled', 'fista', 'unet']
        
        print(f"\nRunning comprehensive experiment...")
        print(f"Methods: {methods}")
        print(f"Acceleration factors: {dataset['metadata']['acceleration_factors']}")
        print(f"Number of phantoms: {dataset['metadata']['num_phantoms']}")
        
        # Initialize results structure
        results = {
            'reconstructions': {method: {} for method in methods},
            'metrics': {method: {} for method in methods},
            'method_info': {method: {} for method in methods},
            'summary_statistics': {},
            'metadata': dataset['metadata'].copy()
        }
        
        acceleration_factors = dataset['metadata']['acceleration_factors']
        
        # Process each acceleration factor
        for acc_factor in acceleration_factors:
            print(f"\nProcessing acceleration factor {acc_factor}x...")
            
            # Initialize storage for this acceleration factor
            for method in methods:
                results['reconstructions'][method][acc_factor] = []
                results['metrics'][method][acc_factor] = []
                results['method_info'][method][acc_factor] = []
            
            preprocessed_data = dataset['preprocessed_data'][acc_factor]
            
            # Process each phantom
            for phantom_idx, phantom_data in enumerate(preprocessed_data):
                print(f"  Phantom {phantom_idx + 1}/{len(preprocessed_data)}")
                
                ground_truth = phantom_data['phantom_info']['image']
                kspace_data = phantom_data['kspace_data']
                
                # Reconstruct with each method
                phantom_reconstructions = {}
                
                for method in methods:
                    reconstruction, method_info = self.reconstruct_with_method(
                        method, kspace_data, ground_truth
                    )
                    
                    # Store results
                    results['reconstructions'][method][acc_factor].append(reconstruction)
                    results['method_info'][method][acc_factor].append(method_info)
                    
                    phantom_reconstructions[method] = reconstruction
                
                # Compute metrics for this phantom
                phantom_metrics = compare_reconstructions(
                    ground_truth,
                    phantom_reconstructions,
                    metrics=['mse', 'rmse', 'mae', 'psnr', 'ssim', 'nrmse', 'correlation']
                )
                
                # Store metrics
                for method in methods:
                    if method in phantom_metrics:
                        results['metrics'][method][acc_factor].append(phantom_metrics[method])
        
        # Compute summary statistics
        results['summary_statistics'] = self._compute_summary_statistics(results)
        
        # Save results if requested
        if save_results:
            self._save_experiment_results(results, output_dir)
        
        print(f"\nExperiment complete!")
        return results
    
    def _compute_summary_statistics(self, results: Dict) -> Dict:
        """Compute summary statistics across all experiments"""
        summary = {
            'mean_metrics': {},
            'std_metrics': {},
            'best_methods': {},
            'timing_analysis': {}
        }
        
        methods = list(results['metrics'].keys())
        acceleration_factors = list(results['metrics'][methods[0]].keys())
        
        # Compute statistics for each method and acceleration factor
        for method in methods:
            summary['mean_metrics'][method] = {}
            summary['std_metrics'][method] = {}
            summary['timing_analysis'][method] = {}
            
            for acc_factor in acceleration_factors:
                method_metrics = results['metrics'][method][acc_factor]
                method_info = results['method_info'][method][acc_factor]
                
                # Aggregate metrics
                summary['mean_metrics'][method][acc_factor] = {}
                summary['std_metrics'][method][acc_factor] = {}
                
                if method_metrics:
                    # Get all metric names
                    metric_names = method_metrics[0].keys()
                    
                    for metric in metric_names:
                        values = [metrics[metric] for metrics in method_metrics 
                                 if metric in metrics and np.isfinite(metrics[metric])]
                        
                        if values:
                            summary['mean_metrics'][method][acc_factor][metric] = np.mean(values)
                            summary['std_metrics'][method][acc_factor][metric] = np.std(values)
                
                # Timing analysis
                timing_values = [info['reconstruction_time'] for info in method_info]
                if timing_values:
                    summary['timing_analysis'][method][acc_factor] = {
                        'mean_time': np.mean(timing_values),
                        'std_time': np.std(timing_values),
                        'total_time': np.sum(timing_values)
                    }
        
        # Determine best methods for each metric and acceleration factor
        for acc_factor in acceleration_factors:
            summary['best_methods'][acc_factor] = {}
            
            metric_names = ['mse', 'rmse', 'mae', 'psnr', 'ssim', 'nrmse', 'correlation']
            
            for metric in metric_names:
                best_method = None
                best_value = float('inf') if metric in ['mse', 'rmse', 'mae', 'nrmse'] else float('-inf')
                
                for method in methods:
                    if (acc_factor in summary['mean_metrics'][method] and 
                        metric in summary['mean_metrics'][method][acc_factor]):
                        
                        value = summary['mean_metrics'][method][acc_factor][metric]
                        
                        if metric in ['mse', 'rmse', 'mae', 'nrmse']:  # Lower is better
                            if value < best_value:
                                best_value = value
                                best_method = method
                        else:  # Higher is better
                            if value > best_value:
                                best_value = value
                                best_method = method
                
                summary['best_methods'][acc_factor][metric] = best_method
        
        return summary
    
    def _save_experiment_results(self, results: Dict, output_dir: str):
        """Save experiment results to files"""
        print(f"\nSaving results to {output_dir}/...")
        
        # Create output directory
        from pathlib import Path
        output_path = Path(output_dir)
        output_path.mkdir(exist_ok=True)
        
        # Save a simple summary
        summary_file = output_path / "experiment_summary.txt"
        
        with open(summary_file, 'w') as f:
            f.write("MRI Reconstruction Results Summary\n")
            f.write("=" * 50 + "\n\n")
            
            # Experiment details
            if 'metadata' in results:
                f.write("Experiment Configuration:\n")
                f.write(f"  Image size: {results['metadata'].get('image_size', 'N/A')}\n")
                f.write(f"  Acceleration factors: {results['metadata'].get('acceleration_factors', 'N/A')}\n")
                f.write(f"  Number of phantoms: {results['metadata'].get('num_phantoms', 'N/A')}\n\n")
            
            # Methods comparison
            if 'metrics' in results:
                f.write("Methods Evaluated:\n")
                methods = list(results['metrics'].keys())
                for method in methods:
                    f.write(f"  - {method.replace('_', ' ').title()}\n")
                f.write("\n")
            
            f.write("Detailed results computed successfully.\n")
        
        print(f"Results summary saved to: {summary_file}")
    
    def display_results_summary(self, results: Dict):
        """Display comprehensive results summary"""
        summary = results['summary_statistics']
        
        print("\n" + "="*80)
        print("COMPREHENSIVE MRI RECONSTRUCTION EXPERIMENT RESULTS")
        print("="*80)
        
        methods = list(summary['mean_metrics'].keys())
        acceleration_factors = list(summary['mean_metrics'][methods[0]].keys())
        
        for acc_factor in acceleration_factors:
            print(f"\nAcceleration Factor: {acc_factor}x")
            print("-" * 70)
            
            # Metrics table
            print(f"{'Method':<12} {'PSNR (dB)':<10} {'SSIM':<8} {'MSE':<10} {'Time (s)':<10}")
            print("-" * 70)
            
            for method in methods:
                if acc_factor in summary['mean_metrics'][method]:
                    metrics = summary['mean_metrics'][method][acc_factor]
                    timing = summary['timing_analysis'][method][acc_factor]
                    
                    psnr = metrics.get('psnr', 0)
                    ssim = metrics.get('ssim', 0)
                    mse = metrics.get('mse', 0)
                    time_val = timing.get('mean_time', 0)
                    
                    print(f"{method:<12} {psnr:<10.2f} {ssim:<8.4f} {mse:<10.6f} {time_val:<10.4f}")
            
            # Best methods
            print(f"\nBest performing methods:")
            if acc_factor in summary['best_methods']:
                best = summary['best_methods'][acc_factor]
                for metric, method in best.items():
                    if method:
                        print(f"  {metric.upper()}: {method}")


def main():
    """Run complete pipeline demonstration"""
    print("🔬 Comprehensive MRI Reconstruction Pipeline")
    print("=" * 60)
    
    # Initialize pipeline
    pipeline = MRIReconstructionPipeline(image_size=(64, 64))  # Smaller for faster testing
    
    # Generate experiment dataset
    dataset = pipeline.generate_experiment_data(
        num_phantoms=2,  # Fewer phantoms for faster testing
        acceleration_factors=[2.0, 4.0]  # Just 2 acceleration factors
    )
    
    # Run comprehensive experiment
    results = pipeline.run_comprehensive_experiment(
        dataset=dataset,
        methods=['zero_filled', 'fista', 'unet'],
        save_results=True,
        output_dir="experiment_results"
    )
    
    # Display results
    pipeline.display_results_summary(results)
    
    print(f"\n🎉 Comprehensive pipeline demonstration complete!")
    print(f"📊 Processed {dataset['metadata']['num_phantoms']} phantoms")
    print(f"🔄 Tested {len(dataset['metadata']['acceleration_factors'])} acceleration factors")
    print(f"🧠 Compared {len(results['reconstructions'])} reconstruction methods")
    print(f"💾 Results saved to experiment_results/")


if __name__ == "__main__":
    main()
 