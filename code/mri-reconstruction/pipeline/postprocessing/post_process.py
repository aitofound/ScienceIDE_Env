"""
Postprocessing utilities for MRI reconstruction pipeline
Handles result analysis, visualization, and quality assessment
"""

import numpy as np
import matplotlib.pyplot as plt
from typing import Dict, List, Optional, Tuple, Any
import sys
import os
from pathlib import Path

# Add project root to path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from evaluation.metrics.reconstruction_metrics import compare_reconstructions, ReconstructionMetrics


class MRIPostprocessor:
    """Postprocessing utilities for MRI reconstruction results"""
    
    @staticmethod
    def create_comparison_figure(ground_truth: np.ndarray,
                               reconstructions: Dict[str, np.ndarray],
                               title: str = "MRI Reconstruction Comparison",
                               save_path: Optional[str] = None,
                               figsize: Tuple[int, int] = (15, 10)) -> plt.Figure:
        """
        Create comparison figure showing ground truth and reconstructions
        
        Args:
            ground_truth: Ground truth image
            reconstructions: Dictionary of {method_name: reconstructed_image}
            title: Figure title
            save_path: Path to save figure (optional)
            figsize: Figure size
            
        Returns:
            Matplotlib figure object
        """
        n_methods = len(reconstructions)
        n_cols = min(4, n_methods + 1)  # +1 for ground truth
        n_rows = (n_methods + 1 + n_cols - 1) // n_cols  # Ceiling division
        
        fig, axes = plt.subplots(n_rows, n_cols, figsize=figsize)
        if n_rows == 1:
            axes = axes.reshape(1, -1)
        
        # Flatten axes for easier indexing
        axes_flat = axes.flatten()
        
        # Display ground truth
        im0 = axes_flat[0].imshow(ground_truth, cmap='gray', vmin=0, vmax=1)
        axes_flat[0].set_title('Ground Truth', fontsize=12, fontweight='bold')
        axes_flat[0].axis('off')
        plt.colorbar(im0, ax=axes_flat[0], fraction=0.046, pad=0.04)
        
        # Display reconstructions
        for idx, (method_name, reconstruction) in enumerate(reconstructions.items()):
            ax_idx = idx + 1
            im = axes_flat[ax_idx].imshow(reconstruction, cmap='gray', vmin=0, vmax=1)
            axes_flat[ax_idx].set_title(method_name.replace('_', ' ').title(), 
                                      fontsize=12, fontweight='bold')
            axes_flat[ax_idx].axis('off')
            plt.colorbar(im, ax=axes_flat[ax_idx], fraction=0.046, pad=0.04)
        
        # Hide unused subplots
        for idx in range(n_methods + 1, len(axes_flat)):
            axes_flat[idx].axis('off')
        
        plt.suptitle(title, fontsize=16, fontweight='bold')
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Figure saved to: {save_path}")
        
        return fig
    
    @staticmethod
    def create_error_maps(ground_truth: np.ndarray,
                         reconstructions: Dict[str, np.ndarray],
                         title: str = "Reconstruction Error Maps",
                         save_path: Optional[str] = None) -> plt.Figure:
        """
        Create error maps showing reconstruction errors
        
        Args:
            ground_truth: Ground truth image
            reconstructions: Dictionary of reconstructions
            title: Figure title
            save_path: Path to save figure
            
        Returns:
            Matplotlib figure object
        """
        n_methods = len(reconstructions)
        n_cols = min(3, n_methods)
        n_rows = (n_methods + n_cols - 1) // n_cols
        
        fig, axes = plt.subplots(n_rows, n_cols, figsize=(4*n_cols, 4*n_rows))
        if n_methods == 1:
            axes = [axes]
        elif n_rows == 1:
            axes = axes.reshape(1, -1)
        
        axes_flat = axes.flatten() if n_methods > 1 else axes
        
        for idx, (method_name, reconstruction) in enumerate(reconstructions.items()):
            error_map = np.abs(ground_truth - reconstruction)
            
            im = axes_flat[idx].imshow(error_map, cmap='hot', vmin=0, vmax=error_map.max())
            axes_flat[idx].set_title(f'{method_name.replace("_", " ").title()}\nMAE: {np.mean(error_map):.4f}')
            axes_flat[idx].axis('off')
            plt.colorbar(im, ax=axes_flat[idx], fraction=0.046, pad=0.04)
        
        # Hide unused subplots
        if n_methods > 1:
            for idx in range(n_methods, len(axes_flat)):
                axes_flat[idx].axis('off')
        
        plt.suptitle(title, fontsize=14, fontweight='bold')
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
        
        return fig
    
    @staticmethod
    def create_metrics_table(ground_truth: np.ndarray,
                           reconstructions: Dict[str, np.ndarray],
                           metrics: List[str] = None) -> Dict[str, Dict[str, float]]:
        """
        Create comprehensive metrics table
        
        Args:
            ground_truth: Ground truth image
            reconstructions: Dictionary of reconstructions
            metrics: List of metrics to compute
            
        Returns:
            Dictionary of metrics for each method
        """
        if metrics is None:
            metrics = ['mse', 'rmse', 'mae', 'psnr', 'ssim', 'nrmse', 'correlation']
        
        results = compare_reconstructions(ground_truth, reconstructions, metrics)
        
        return results
    
    @staticmethod
    def print_metrics_table(metrics_results: Dict[str, Dict[str, float]],
                          title: str = "Reconstruction Quality Metrics"):
        """
        Print formatted metrics table
        
        Args:
            metrics_results: Results from create_metrics_table()
            title: Table title
        """
        print(f"\n{title}")
        print("=" * len(title))
        
        # Get all metrics
        all_metrics = set()
        for method_metrics in metrics_results.values():
            all_metrics.update(method_metrics.keys())
        
        metric_order = ['mse', 'rmse', 'mae', 'psnr', 'ssim', 'nrmse', 'snr', 'correlation']
        ordered_metrics = [m for m in metric_order if m in all_metrics]
        ordered_metrics.extend([m for m in all_metrics if m not in metric_order])
        
        # Print header
        header = f"{'Method':<15}"
        for metric in ordered_metrics:
            header += f"{metric.upper():<12}"
        print(header)
        print("-" * len(header))
        
        # Print results for each method
        for method, metrics in metrics_results.items():
            row = f"{method:<15}"
            for metric in ordered_metrics:
                value = metrics.get(metric, 0)
                if metric in ['psnr', 'snr']:
                    row += f"{value:<12.2f}"
                elif metric in ['ssim', 'correlation']:
                    row += f"{value:<12.4f}"
                else:
                    row += f"{value:<12.6f}"
            print(row)
    
    @staticmethod
    def create_convergence_plot(convergence_history: Dict[str, List],
                              title: str = "Algorithm Convergence",
                              save_path: Optional[str] = None) -> plt.Figure:
        """
        Create convergence plot for iterative algorithms
        
        Args:
            convergence_history: Dictionary with 'cost', 'iterations' etc.
            title: Plot title
            save_path: Path to save figure
            
        Returns:
            Matplotlib figure object
        """
        fig, axes = plt.subplots(2, 2, figsize=(12, 8))
        
        # Cost evolution
        if 'cost' in convergence_history:
            axes[0, 0].plot(convergence_history['cost'])
            axes[0, 0].set_title('Total Cost')
            axes[0, 0].set_xlabel('Iteration')
            axes[0, 0].set_ylabel('Cost')
            axes[0, 0].grid(True)
        
        # Data fidelity
        if 'data_fidelity' in convergence_history:
            axes[0, 1].plot(convergence_history['data_fidelity'])
            axes[0, 1].set_title('Data Fidelity')
            axes[0, 1].set_xlabel('Iteration')
            axes[0, 1].set_ylabel('Data Fidelity')
            axes[0, 1].grid(True)
        
        # Regularization
        if 'regularization' in convergence_history:
            axes[1, 0].plot(convergence_history['regularization'])
            axes[1, 0].set_title('Regularization')
            axes[1, 0].set_xlabel('Iteration')
            axes[1, 0].set_ylabel('Regularization')
            axes[1, 0].grid(True)
        
        # Step size
        if 'step_size' in convergence_history:
            axes[1, 1].plot(convergence_history['step_size'])
            axes[1, 1].set_title('Step Size')
            axes[1, 1].set_xlabel('Iteration')
            axes[1, 1].set_ylabel('Step Size')
            axes[1, 1].grid(True)
        
        plt.suptitle(title, fontsize=14, fontweight='bold')
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
        
        return fig
    
    @staticmethod
    def save_results_summary(results: Dict, 
                           output_dir: str = "results",
                           experiment_name: str = "mri_reconstruction") -> str:
        """
        Save comprehensive results summary
        
        Args:
            results: Complete results dictionary
            output_dir: Output directory
            experiment_name: Experiment name for file naming
            
        Returns:
            Path to saved summary file
        """
        output_path = Path(output_dir)
        output_path.mkdir(exist_ok=True)
        
        summary_file = output_path / f"{experiment_name}_summary.txt"
        
        with open(summary_file, 'w') as f:
            f.write(f"MRI Reconstruction Results Summary\n")
            f.write(f"=" * 50 + "\n\n")
            
            # Experiment details
            if 'metadata' in results:
                f.write("Experiment Configuration:\n")
                f.write(f"  Image size: {results['metadata'].get('image_size', 'N/A')}\n")
                f.write(f"  Acceleration factors: {results['metadata'].get('acceleration_factors', 'N/A')}\n")
                f.write(f"  Noise levels: {results['metadata'].get('noise_levels', 'N/A')}\n\n")
            
            # Methods comparison
            if 'metrics' in results:
                f.write("Methods Evaluated:\n")
                methods = list(results['metrics'].keys())
                for method in methods:
                    f.write(f"  - {method.replace('_', ' ').title()}\n")
                f.write("\n")
            
            # Performance summary would go here
            f.write("Detailed results saved in accompanying files.\n")
        
        print(f"Results summary saved to: {summary_file}")
        return str(summary_file)


def main():
    """Test postprocessing utilities"""
    print("Testing MRI postprocessing utilities...")
    
    # Generate test data
    from data.data_generator import SyntheticMRIGenerator
    generator = SyntheticMRIGenerator(image_size=(64, 64))
    ground_truth = generator.generate_shepp_logan()
    
    # Create fake reconstructions for testing
    reconstructions = {
        'zero_filled': ground_truth + 0.1 * np.random.randn(*ground_truth.shape),
        'fista': ground_truth + 0.05 * np.random.randn(*ground_truth.shape),
        'unet': ground_truth + 0.02 * np.random.randn(*ground_truth.shape)
    }
    
    postprocessor = MRIPostprocessor()
    
    # Test metrics table
    metrics_results = postprocessor.create_metrics_table(ground_truth, reconstructions)
    postprocessor.print_metrics_table(metrics_results)
    
    # Test figure creation (comment out plt.show() calls for automated testing)
    print("\nCreating comparison figure...")
    fig1 = postprocessor.create_comparison_figure(ground_truth, reconstructions)
    plt.close(fig1)  # Close to avoid display in testing
    
    print("Creating error maps...")
    fig2 = postprocessor.create_error_maps(ground_truth, reconstructions)
    plt.close(fig2)  # Close to avoid display in testing
    
    print("Postprocessing utilities working correctly!")


if __name__ == "__main__":
    main()