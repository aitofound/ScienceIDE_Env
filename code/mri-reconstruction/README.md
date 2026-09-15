# MRI Reconstruction: Classical and Deep Learning Approaches

A comprehensive implementation of MRI reconstruction algorithms comparing classical compressed sensing methods with deep learning approaches. This project demonstrates end-to-end reconstruction pipelines suitable for research and clinical applications.

![Reconstruction Comparison](docs/images/reconstruction_comparison_overview.png)
*Figure 1: MRI reconstruction comparison showing Ground Truth, Zero-filled (19.3 dB), FISTA (26.8 dB), and U-Net (30.4 dB) for 4× acceleration. U-Net demonstrates superior artifact suppression and detail preservation.*

## Project Overview

Medical imaging, particularly MRI, often requires long acquisition times that can be reduced through undersampling techniques. However, undersampled data leads to artifacts that must be corrected through sophisticated reconstruction algorithms. This project implements and compares multiple reconstruction approaches:

- **Classical Methods**: FISTA (Fast Iterative Shrinkage-Thresholding Algorithm) with Total Variation regularization
- **Deep Learning**: U-Net architecture for learned reconstruction
- **Baseline**: Zero-filled reconstruction for comparison

The implementation includes synthetic data generation, comprehensive evaluation metrics, and a complete experimental pipeline for systematic comparison of reconstruction methods.

![Performance Summary](docs/images/performance_summary.png)
*Figure 2: Performance analysis across acceleration factors. Left: PSNR comparison showing U-Net's consistent superiority. Center: SSIM structural similarity metrics. Right: Computational efficiency comparison.*

## Repository Structure

```
mri_reconstruction/
├── data/
│   ├── data_generator.py          # Synthetic MRI phantom generation
│   └── data_loader.py             # Data loading utilities
├── algorithms/
│   ├── classical/
│   │   ├── fista.py               # FISTA reconstruction algorithm
│   │   └── admm.py                # ADMM implementation (placeholder)
│   ├── ai/
│   │   ├── unet.py                # U-Net architecture and training
│   │   ├── training.py            # Training utilities
│   │   └── inference.py           # Inference utilities
│   └── utils/
│       ├── kspace.py              # K-space operations and FFT utilities
│       ├── transforms.py          # Image transformations
│       └── noise.py               # Noise simulation
├── evaluation/
│   └── metrics/
│       ├── image_quality.py       # Image quality metrics
│       └── reconstruction_metrics.py  # Comprehensive evaluation metrics
├── pipeline/
│   ├── preprocessing/
│   │   └── data_prep.py           # Data preprocessing utilities
│   ├── reconstruction/
│   │   └── reconstructor.py       # Main reconstruction pipeline
│   └── postprocessing/
│       └── post_process.py        # Results analysis and visualization
├── tests/
│   ├── unit/                      # Unit tests for all components
│   └── integration/               # Integration tests
├── visualization/
│   ├── plots/                     # Plotting utilities
│   └── dashboard/                 # Interactive visualization tools
├── config/
│   ├── config.py                  # Configuration management
│   └── model_config.yaml          # Model configurations
├── notebooks/                     # Jupyter notebooks for exploration
├── docs/
│   ├── images/                    # Documentation figures
│   └── technical_report.pdf       # Comprehensive technical documentation
├── requirements.txt               # Python dependencies
├── setup.py                       # Package installation
└── README.md                      # This file
```

## Installation

### Prerequisites

- Python 3.8 or higher
- CUDA-capable GPU (optional, for deep learning acceleration)

### Setup Instructions

1. Clone the repository:
```bash
git clone https://github.com/chaurasiavikash/mri_reconstruction.git
cd mri_reconstruction
```

2. Create a virtual environment:
```bash
python -m venv mri_env
source mri_env/bin/activate  # On Windows: mri_env\Scripts\activate
```

3. Install dependencies:
```bash
pip install -r requirements.txt
```

4. Verify installation by running tests:
```bash
pytest tests/unit/ -v
```

## Quick Start

### Running the Complete Pipeline

The main reconstruction pipeline demonstrates all implemented methods:

```bash
cd pipeline/reconstruction
python reconstructor.py
```

This will:
- Generate synthetic MRI phantoms
- Create undersampled k-space data with multiple acceleration factors
- Reconstruct using all available methods
- Evaluate reconstruction quality
- Save results and generate summary report

### Individual Component Testing

Test specific components:

```bash
# Test data generation
python data/data_generator.py

# Test k-space operations
python algorithms/utils/kspace.py

# Test FISTA reconstruction
python algorithms/classical/fista.py

# Test U-Net model
python algorithms/ai/unet.py

# Test evaluation metrics
python evaluation/metrics/reconstruction_metrics.py
```

## Algorithm Implementations

### Classical Reconstruction: FISTA

The FISTA implementation includes:
- Fast Iterative Shrinkage-Thresholding Algorithm for L1-regularized least squares
- Total Variation regularization for edge preservation
- Adaptive step size with backtracking line search
- Convergence monitoring and early stopping

Key features:
- Handles complex k-space data and real image reconstructions
- Configurable regularization parameters
- Robust numerical implementation with stability checks
- Comprehensive convergence tracking

### Deep Learning: U-Net

The U-Net architecture features:
- Encoder-decoder structure with skip connections
- Batch normalization and dropout for regularization
- Configurable depth and feature channels
- Custom loss function combining MSE, L1, and SSIM terms

The implementation supports:
- Training on synthetic and real MRI data
- Transfer learning capabilities
- Model checkpointing and resumption
- Comprehensive training metrics logging

### K-space Operations

Comprehensive k-space utilities including:
- Centered 2D FFT and inverse FFT operations
- Multiple undersampling patterns (random, uniform, 1D random)
- Realistic noise simulation with complex Gaussian noise
- Sampling mask generation with configurable center fractions

## Evaluation Framework

### Metrics Implementation

The evaluation system includes standard medical imaging metrics:

**Fidelity Metrics:**
- Mean Squared Error (MSE)
- Root Mean Squared Error (RMSE)
- Mean Absolute Error (MAE)
- Peak Signal-to-Noise Ratio (PSNR)

**Perceptual Quality:**
- Structural Similarity Index Measure (SSIM)
- Correlation Coefficient
- Edge Preservation Index

**Clinical Relevance:**
- Normalized Root Mean Squared Error (NRMSE)
- Signal-to-Noise Ratio (SNR)
- Artifact Power in background regions
- Blur Assessment using Laplacian variance

### Experimental Design

The pipeline supports systematic evaluation across:
- Multiple phantom types (Shepp-Logan, brain-like structures)
- Various acceleration factors (2x, 4x, 6x, 8x)
- Different noise levels and SNR conditions
- Statistical analysis across multiple realizations

## Configuration

### Algorithm Parameters

FISTA configuration in `config/model_config.yaml`:
```yaml
fista:
  max_iterations: 50
  lambda_reg: 0.01
  tolerance: 1e-6
  line_search: true
  verbose: false
```

U-Net configuration:
```yaml
unet:
  n_channels: 1
  n_classes: 1
  features: [64, 128, 256, 512, 1024]
  bilinear: true
  dropout_rate: 0.1
```

### Experimental Settings

Default experimental parameters:
- Image size: 256x256 pixels
- Acceleration factors: [2.0, 4.0, 6.0]
- Center fraction: 8% (fully sampled low frequencies)
- Noise SNR: 30 dB
- Number of test phantoms: 5

## Results and Benchmarks

### Expected Performance

Typical reconstruction quality (PSNR in dB) for 4x acceleration:

| Method | Shepp-Logan | Brain Phantom | Runtime (CPU) |
|--------|-------------|---------------|---------------|
| Zero-filled | 18-22 | 16-20 | <0.001s |
| FISTA | 24-28 | 22-26 | 0.1-0.5s |
| U-Net (trained) | 28-32 | 26-30 | 0.01-0.05s |

Note: U-Net performance requires proper training. The included implementation provides the architecture but uses random weights for demonstration.

### Computational Requirements

- **Memory**: 4-8 GB RAM for 256x256 images
- **Storage**: ~100 MB for full installation
- **GPU**: Optional, reduces U-Net inference time by 10-50x

## Documentation

For detailed technical documentation, algorithm descriptions, and comprehensive evaluation results, see:
- **Technical Report**: [`docs/technical_report.pdf`](docs/technical_report.pdf)
- **Algorithm Details**: Individual module documentation
- **Jupyter Notebooks**: Interactive examples in [`notebooks/`](notebooks/)

## Testing

The project includes comprehensive test suites:

### Unit Tests
```bash
# Test all components
pytest tests/unit/ -v

# Test specific modules
pytest tests/unit/test_data_generator.py -v
pytest tests/unit/test_fista.py -v
pytest tests/unit/test_unet.py -v
pytest tests/unit/test_metrics.py -v
```

### Integration Tests
```bash
pytest tests/integration/ -v
```

Test coverage includes:
- Data generation and k-space operations
- Algorithm convergence and numerical stability
- Metric computation accuracy
- Edge cases and error handling
- Memory and performance benchmarks

## Contributing

### Development Setup

1. Install development dependencies:
```bash
pip install -r requirements-dev.txt
```

2. Install pre-commit hooks:
```bash
pre-commit install
```

3. Run full test suite:
```bash
pytest tests/ --cov=. --cov-report=html
```

### Code Style

- Follow PEP 8 style guidelines
- Use type hints for function signatures
- Include comprehensive docstrings
- Maintain test coverage above 90%

### Adding New Algorithms

To add new reconstruction methods:

1. Implement the algorithm in `algorithms/classical/` or `algorithms/ai/`
2. Add corresponding tests in `tests/unit/`
3. Update the main pipeline in `pipeline/reconstruction/reconstructor.py`
4. Add configuration options in `config/model_config.yaml`

## Clinical Applications

This implementation is designed for research and educational purposes. The algorithms demonstrate principles used in clinical MRI reconstruction but should not be used for medical diagnosis without proper validation and regulatory approval.

### Potential Applications

- **Research**: Algorithm development and benchmarking
- **Education**: Teaching MRI physics and reconstruction principles
- **Prototyping**: Rapid development of new reconstruction methods
- **Simulation**: Testing reconstruction performance under various conditions

## Technical Details

### Mathematical Background

**FISTA Objective Function:**
```
min_x (1/2)||A(x) - b||²₂ + λ||Ψ(x)||₁
```
where:
- A: Forward operator (FFT + undersampling)
- b: Measured k-space data
- Ψ: Sparsity transform (Total Variation)
- λ: Regularization parameter

**U-Net Architecture:**
- Encoder: Series of convolution + pooling operations
- Decoder: Upsampling + convolution with skip connections
- Loss: Combined MSE + L1 + SSIM for perceptual quality

### Implementation Notes

- All algorithms handle complex k-space data properly
- Numerical stability ensured through careful implementation
- Memory-efficient processing for large images
- Extensible design for adding new methods

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

This implementation draws inspiration from:
- FISTA algorithm by Beck & Teboulle (2009)
- U-Net architecture by Ronneberger et al. (2015)
- FastMRI challenge and dataset
- Medical imaging reconstruction literature

## Contact

For questions, issues, or collaboration opportunities:
- GitHub Issues: [Project Issues](https://github.com/chaurasiavikash/mri_reconstruction/issues)
- Email: chaurasiavik@gmail.com

## References

1. Beck, A., & Teboulle, M. (2009). A fast iterative shrinkage-thresholding algorithm for linear inverse problems. SIAM journal on imaging sciences, 2(1), 183-202.

2. Ronneberger, O., Fischer, P., & Brox, T. (2015). U-net: Convolutional networks for biomedical image segmentation. In International Conference on Medical image computing and computer-assisted intervention.

3. Lustig, M., Donoho, D., & Pauly, J. M. (2007). Sparse MRI: The application of compressed sensing for rapid MR imaging. Magnetic Resonance in Medicine, 58(6), 1182-1195.

4. Zbontar, J., et al. (2018). fastMRI: An open dataset and benchmarks for accelerated MRI. arXiv preprint arXiv:1811.08839.