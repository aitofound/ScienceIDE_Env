"""
Preprocessing utilities for MRI reconstruction pipeline
Handles data preparation, normalization, and k-space preparation
"""

import numpy as np
from typing import Tuple, Optional, Dict, Any
import sys
import os

# Add project root to path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from algorithms.utils.kspace import KSpaceUtils


class MRIPreprocessor:
    """Preprocessing utilities for MRI data"""
    
    @staticmethod
    def normalize_image(image: np.ndarray, 
                       method: str = 'minmax',
                       percentiles: Tuple[float, float] = (1, 99)) -> Tuple[np.ndarray, Dict[str, Any]]:
        """
        Normalize MRI image
        
        Args:
            image: Input MRI image
            method: Normalization method ('minmax', 'zscore', 'percentile')
            percentiles: Percentiles for percentile normalization
            
        Returns:
            Tuple of (normalized_image, normalization_params)
        """
        params = {'method': method}
        
        if method == 'minmax':
            min_val = image.min()
            max_val = image.max()
            normalized = (image - min_val) / (max_val - min_val + 1e-8)
            params.update({'min': min_val, 'max': max_val})
            
        elif method == 'zscore':
            mean_val = image.mean()
            std_val = image.std()
            normalized = (image - mean_val) / (std_val + 1e-8)
            params.update({'mean': mean_val, 'std': std_val})
            
        elif method == 'percentile':
            p_low, p_high = np.percentile(image, percentiles)
            normalized = np.clip((image - p_low) / (p_high - p_low + 1e-8), 0, 1)
            params.update({'p_low': p_low, 'p_high': p_high, 'percentiles': percentiles})
            
        else:
            raise ValueError(f"Unknown normalization method: {method}")
        
        return normalized, params
    
    @staticmethod
    def denormalize_image(normalized_image: np.ndarray, 
                         params: Dict[str, Any]) -> np.ndarray:
        """
        Reverse normalization
        
        Args:
            normalized_image: Normalized image
            params: Normalization parameters from normalize_image()
            
        Returns:
            Denormalized image
        """
        method = params['method']
        
        if method == 'minmax':
            return normalized_image * (params['max'] - params['min']) + params['min']
            
        elif method == 'zscore':
            return normalized_image * params['std'] + params['mean']
            
        elif method == 'percentile':
            return normalized_image * (params['p_high'] - params['p_low']) + params['p_low']
            
        else:
            raise ValueError(f"Unknown normalization method: {method}")
    
    @staticmethod
    def prepare_kspace_data(image: np.ndarray,
                           acceleration_factor: float = 4.0,
                           center_fraction: float = 0.08,
                           sampling_pattern: str = '1d_random',
                           noise_snr_db: Optional[float] = None) -> Dict[str, np.ndarray]:
        """
        Prepare k-space data with undersampling and noise
        
        Args:
            image: Ground truth MRI image
            acceleration_factor: Undersampling acceleration factor
            center_fraction: Fraction of center k-space to fully sample
            sampling_pattern: Sampling pattern type
            noise_snr_db: SNR for noise addition (None for no noise)
            
        Returns:
            Dictionary containing k-space data and metadata
        """
        # Convert to k-space
        kspace_full = KSpaceUtils.fft2c(image)
        
        # Create sampling mask
        mask = KSpaceUtils.create_sampling_mask(
            kspace_full.shape,
            acceleration_factor=acceleration_factor,
            center_fraction=center_fraction,
            pattern=sampling_pattern
        )
        
        # Apply undersampling
        kspace_undersampled = KSpaceUtils.apply_sampling_mask(kspace_full, mask)
        
        # Add noise if specified
        if noise_snr_db is not None:
            kspace_undersampled = KSpaceUtils.add_noise_to_kspace(
                kspace_undersampled, snr_db=noise_snr_db
            )
        
        # Zero-filled reconstruction for reference
        zero_filled = KSpaceUtils.zero_fill_reconstruction(kspace_undersampled)
        
        return {
            'kspace_full': kspace_full,
            'kspace_undersampled': kspace_undersampled,
            'sampling_mask': mask,
            'zero_filled': zero_filled,
            'sampling_percentage': KSpaceUtils.calculate_sampling_percentage(mask),
            'acceleration_factor': acceleration_factor,
            'center_fraction': center_fraction,
            'sampling_pattern': sampling_pattern,
            'noise_snr_db': noise_snr_db
        }
    
    @staticmethod
    def crop_or_pad_image(image: np.ndarray, 
                         target_size: Tuple[int, int]) -> np.ndarray:
        """
        Crop or pad image to target size
        
        Args:
            image: Input image
            target_size: Target (height, width)
            
        Returns:
            Resized image
        """
        h, w = image.shape
        target_h, target_w = target_size
        
        # Calculate padding/cropping
        if h < target_h:
            # Pad height
            pad_h = target_h - h
            pad_top = pad_h // 2
            pad_bottom = pad_h - pad_top
        else:
            # Crop height
            crop_h = h - target_h
            crop_top = crop_h // 2
            crop_bottom = crop_h - crop_top
            pad_top = -crop_top
            pad_bottom = -crop_bottom
        
        if w < target_w:
            # Pad width
            pad_w = target_w - w
            pad_left = pad_w // 2
            pad_right = pad_w - pad_left
        else:
            # Crop width
            crop_w = w - target_w
            crop_left = crop_w // 2
            crop_right = crop_w - crop_left
            pad_left = -crop_left
            pad_right = -crop_right
        
        # Apply padding (negative values will be handled as cropping)
        if pad_top >= 0 and pad_bottom >= 0 and pad_left >= 0 and pad_right >= 0:
            # Pure padding case
            result = np.pad(image, ((pad_top, pad_bottom), (pad_left, pad_right)), mode='constant')
        else:
            # Cropping or mixed case
            start_h = max(0, -pad_top)
            end_h = h + min(0, pad_bottom)
            start_w = max(0, -pad_left)
            end_w = w + min(0, pad_right)
            
            cropped = image[start_h:end_h, start_w:end_w]
            
            # Apply any remaining padding
            final_pad_top = max(0, pad_top)
            final_pad_bottom = max(0, pad_bottom)
            final_pad_left = max(0, pad_left)
            final_pad_right = max(0, pad_right)
            
            result = np.pad(cropped, 
                          ((final_pad_top, final_pad_bottom), 
                           (final_pad_left, final_pad_right)), mode='constant')
        
        return result
    
    @staticmethod
    def validate_image_data(image: np.ndarray) -> Dict[str, Any]:
        """
        Validate and analyze image data quality
        
        Args:
            image: Input image
            
        Returns:
            Dictionary with validation results
        """
        validation = {
            'shape': image.shape,
            'dtype': image.dtype,
            'min_value': image.min(),
            'max_value': image.max(),
            'mean_value': image.mean(),
            'std_value': image.std(),
            'has_nan': np.any(np.isnan(image)),
            'has_inf': np.any(np.isinf(image)),
            'is_valid': True,
            'warnings': []
        }
        
        # Check for issues
        if validation['has_nan']:
            validation['is_valid'] = False
            validation['warnings'].append("Image contains NaN values")
        
        if validation['has_inf']:
            validation['is_valid'] = False
            validation['warnings'].append("Image contains infinite values")
        
        if validation['std_value'] < 1e-8:
            validation['warnings'].append("Image has very low contrast (std < 1e-8)")
        
        if image.ndim != 2:
            validation['warnings'].append(f"Expected 2D image, got {image.ndim}D")
        
        if image.size == 0:
            validation['is_valid'] = False
            validation['warnings'].append("Image is empty")
        
        return validation


def main():
    """Test preprocessing utilities"""
    print("Testing MRI preprocessing utilities...")
    
    # Generate test image
    from data.data_generator import SyntheticMRIGenerator
    generator = SyntheticMRIGenerator(image_size=(64, 64))
    test_image = generator.generate_shepp_logan()
    
    preprocessor = MRIPreprocessor()
    
    # Test normalization
    normalized, params = preprocessor.normalize_image(test_image, method='minmax')
    denormalized = preprocessor.denormalize_image(normalized, params)
    
    print(f"Original range: [{test_image.min():.3f}, {test_image.max():.3f}]")
    print(f"Normalized range: [{normalized.min():.3f}, {normalized.max():.3f}]")
    print(f"Denormalized range: [{denormalized.min():.3f}, {denormalized.max():.3f}]")
    
    # Test k-space preparation
    kspace_data = preprocessor.prepare_kspace_data(
        test_image, 
        acceleration_factor=4.0,
        noise_snr_db=30.0
    )
    
    print(f"Sampling percentage: {kspace_data['sampling_percentage']:.1f}%")
    print(f"Zero-filled reconstruction shape: {kspace_data['zero_filled'].shape}")
    
    # Test validation
    validation = preprocessor.validate_image_data(test_image)
    print(f"Image validation: {'PASS' if validation['is_valid'] else 'FAIL'}")
    if validation['warnings']:
        print(f"Warnings: {validation['warnings']}")
    
    print("Preprocessing utilities working correctly!")


if __name__ == "__main__":
    main()