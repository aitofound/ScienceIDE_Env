"""
Test suite for U-Net MRI reconstruction model
"""

import pytest
import torch
import numpy as np
import sys
import os

# Add the project root to the path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from algorithms.ai.unet import (
    UNet, DoubleConv, Down, Up, 
    MRIReconstructionDataset, MRIReconstructionLoss,
    create_model
)


class TestUNetComponents:
    """Test individual components of U-Net"""
    
    def test_double_conv(self):
        """Test DoubleConv block"""
        # Test basic functionality
        conv_block = DoubleConv(1, 64)
        test_input = torch.randn(2, 1, 32, 32)
        
        output = conv_block(test_input)
        
        assert output.shape == (2, 64, 32, 32), f"Expected (2, 64, 32, 32), got {output.shape}"
        assert not torch.isnan(output).any(), "Output should not contain NaN"
        assert not torch.isinf(output).any(), "Output should not contain Inf"
        
        # Test different channel configurations
        conv_block_multi = DoubleConv(3, 128, dropout_rate=0.2)
        test_input_multi = torch.randn(1, 3, 64, 64)
        output_multi = conv_block_multi(test_input_multi)
        
        assert output_multi.shape == (1, 128, 64, 64), "Multi-channel input should work"
    
    def test_down_block(self):
        """Test Down (downsampling) block"""
        down_block = Down(64, 128)
        test_input = torch.randn(2, 64, 32, 32)
        
        output = down_block(test_input)
        
        # Should halve spatial dimensions and change channels
        assert output.shape == (2, 128, 16, 16), f"Expected (2, 128, 16, 16), got {output.shape}"
        assert not torch.isnan(output).any(), "Output should not contain NaN"
    
    def test_up_block(self):
        """Test Up (upsampling) block"""
        # Test bilinear upsampling
        # For standard U-Net: in_channels + out_channels after concatenation
        up_block_bilinear = Up(512, 256, bilinear=True)  # 512 + 256 -> 256
        x1 = torch.randn(2, 512, 8, 8)  # Low resolution feature map
        x2 = torch.randn(2, 256, 16, 16)  # Skip connection 
        
        output = up_block_bilinear(x1, x2)
        
        # Should match x2 spatial dimensions and have output channels
        assert output.shape == (2, 256, 16, 16), f"Expected (2, 256, 16, 16), got {output.shape}"
        
        # Test transpose convolution
        up_block_transpose = Up(512, 256, bilinear=False)  
        x1_t = torch.randn(2, 512, 8, 8)
        x2_t = torch.randn(2, 256, 16, 16)  # Skip connection
        output_transpose = up_block_transpose(x1_t, x2_t)
        
        assert output_transpose.shape == (2, 256, 16, 16), "Transpose conv should work too"
    
    def test_up_block_size_mismatch(self):
        """Test Up block handles size mismatches"""
        up_block = Up(512, 256)
        x1 = torch.randn(1, 512, 7, 9)  # Odd dimensions
        x2 = torch.randn(1, 256, 15, 17)  # Different odd dimensions (skip connection)
        
        output = up_block(x1, x2)
        
        # Should handle size mismatch gracefully
        assert output.shape[0] == 1, "Batch dimension should be preserved"
        assert output.shape[1] == 256, "Channel dimension should be correct"


class TestUNetModel:
    """Test complete U-Net model"""
    
    def test_model_creation(self):
        """Test model creation with different configurations"""
        # Default model
        model = UNet()
        assert model.n_channels == 1, "Default input channels should be 1"
        assert model.n_classes == 1, "Default output channels should be 1"
        
        # Custom configuration
        model_custom = UNet(
            n_channels=2, 
            n_classes=2, 
            features=(32, 64, 128, 256, 512),
            bilinear=False,
            dropout_rate=0.2
        )
        assert model_custom.n_channels == 2, "Custom input channels should be set"
        assert model_custom.n_classes == 2, "Custom output channels should be set"
    
    def test_forward_pass(self):
        """Test forward pass with different input sizes"""
        model = UNet()
        
        # Test different input sizes
        test_cases = [
            (1, 1, 64, 64),
            (2, 1, 128, 128),
            (1, 1, 256, 256),
            (3, 1, 32, 32)
        ]
        
        for batch_size, channels, height, width in test_cases:
            test_input = torch.randn(batch_size, channels, height, width)
            
            model.eval()
            with torch.no_grad():
                output = model(test_input)
            
            expected_shape = (batch_size, 1, height, width)
            assert output.shape == expected_shape, \
                f"Input {test_input.shape} should produce output {expected_shape}, got {output.shape}"
            
            # Check output properties
            assert not torch.isnan(output).any(), "Output should not contain NaN"
            assert not torch.isinf(output).any(), "Output should not contain Inf"
    
    def test_model_training_mode(self):
        """Test model behavior in training vs evaluation mode"""
        model = UNet(dropout_rate=0.5)  # High dropout for testing
        test_input = torch.randn(2, 1, 64, 64)
        
        # Training mode - should have some randomness due to dropout
        model.train()
        output1 = model(test_input)
        output2 = model(test_input)
        
        # Outputs should be different due to dropout (with high probability)
        assert not torch.allclose(output1, output2, atol=1e-6), \
            "Training mode outputs should differ due to dropout"
        
        # Evaluation mode - should be deterministic
        model.eval()
        with torch.no_grad():
            output3 = model(test_input)
            output4 = model(test_input)
        
        assert torch.allclose(output3, output4), \
            "Evaluation mode outputs should be identical"
    
    def test_parameter_count(self):
        """Test parameter counting"""
        model = UNet()
        param_count = model.count_parameters()
        
        assert param_count > 0, "Model should have trainable parameters"
        assert isinstance(param_count, int), "Parameter count should be integer"
        
        # Test that it matches manual calculation
        manual_count = sum(p.numel() for p in model.parameters() if p.requires_grad)
        assert param_count == manual_count, "Parameter count should match manual calculation"
    
    def test_gradient_flow(self):
        """Test that gradients flow properly through the network"""
        model = UNet()
        test_input = torch.randn(1, 1, 64, 64, requires_grad=True)
        target = torch.randn(1, 1, 64, 64)
        
        # Forward pass
        output = model(test_input)
        loss = torch.nn.functional.mse_loss(output, target)
        
        # Backward pass
        loss.backward()
        
        # Check that gradients exist and are non-zero
        has_gradients = False
        for param in model.parameters():
            if param.grad is not None and not torch.allclose(param.grad, torch.zeros_like(param.grad)):
                has_gradients = True
                break
        
        assert has_gradients, "Model should have non-zero gradients after backprop"


class TestMRIReconstructionDataset:
    """Test the dataset class"""
    
    def setup_method(self):
        """Setup test data"""
        self.gt_images = np.random.rand(10, 64, 64).astype(np.float32)
        self.us_images = np.random.rand(10, 64, 64).astype(np.float32)
    
    def test_dataset_creation(self):
        """Test dataset creation"""
        dataset = MRIReconstructionDataset(self.gt_images, self.us_images)
        
        assert len(dataset) == 10, "Dataset length should match input"
        
        # Test data access
        input_img, target_img = dataset[0]
        
        assert isinstance(input_img, torch.Tensor), "Input should be tensor"
        assert isinstance(target_img, torch.Tensor), "Target should be tensor"
        assert input_img.shape == (1, 64, 64), "Input should have channel dimension"
        assert target_img.shape == (1, 64, 64), "Target should have channel dimension"
    
    def test_dataset_mismatched_lengths(self):
        """Test dataset with mismatched input lengths"""
        gt_short = np.random.rand(5, 64, 64)
        us_long = np.random.rand(10, 64, 64)
        
        with pytest.raises(AssertionError):
            MRIReconstructionDataset(gt_short, us_long)
    
    def test_dataloader_integration(self):
        """Test integration with PyTorch DataLoader"""
        dataset = MRIReconstructionDataset(self.gt_images, self.us_images)
        dataloader = torch.utils.data.DataLoader(dataset, batch_size=3, shuffle=True)
        
        for batch_inputs, batch_targets in dataloader:
            assert batch_inputs.shape[0] <= 3, "Batch size should not exceed specified"
            assert batch_inputs.shape[1:] == (1, 64, 64), "Sample shape should be preserved"
            assert batch_targets.shape == batch_inputs.shape, "Target shape should match input"
            break  # Only test first batch


class TestMRIReconstructionLoss:
    """Test the loss function"""
    
    def test_loss_computation(self):
        """Test loss computation"""
        loss_fn = MRIReconstructionLoss()
        
        pred = torch.randn(2, 1, 32, 32)
        target = torch.randn(2, 1, 32, 32)
        
        total_loss, loss_dict = loss_fn(pred, target)
        
        # Check return types
        assert isinstance(total_loss, torch.Tensor), "Total loss should be tensor"
        assert isinstance(loss_dict, dict), "Loss dict should be dictionary"
        
        # Check loss components
        expected_keys = ['mse', 'l1', 'ssim', 'total']
        for key in expected_keys:
            assert key in loss_dict, f"Loss dict should contain {key}"
            assert isinstance(loss_dict[key], float), f"{key} should be float"
        
        # Check that total loss is reasonable
        assert total_loss.item() >= 0, "Total loss should be non-negative"
    
    def test_loss_weights(self):
        """Test different loss weights"""
        # Test MSE-only loss
        mse_only = MRIReconstructionLoss(mse_weight=1.0, ssim_weight=0.0, l1_weight=0.0)
        
        # Test SSIM-only loss  
        ssim_only = MRIReconstructionLoss(mse_weight=0.0, ssim_weight=1.0, l1_weight=0.0)
        
        pred = torch.randn(1, 1, 32, 32)
        target = torch.randn(1, 1, 32, 32)
        
        mse_loss, mse_dict = mse_only(pred, target)
        ssim_loss, ssim_dict = ssim_only(pred, target)
        
        # MSE-only should have zero SSIM and L1 contributions
        assert abs(mse_dict['total'] - mse_dict['mse']) < 1e-6, \
            "MSE-only loss should equal MSE component"
    
    def test_perfect_reconstruction_loss(self):
        """Test loss with perfect reconstruction"""
        loss_fn = MRIReconstructionLoss()
        
        target = torch.randn(1, 1, 32, 32)
        pred = target.clone()  # Perfect reconstruction
        
        total_loss, loss_dict = loss_fn(pred, target)
        
        # MSE and L1 should be very close to zero
        assert loss_dict['mse'] < 1e-6, "MSE should be near zero for perfect reconstruction"
        assert loss_dict['l1'] < 1e-6, "L1 should be near zero for perfect reconstruction"


class TestModelIntegration:
    """Test model integration functions"""
    
    def test_create_model_function(self):
        """Test the create_model helper function"""
        model = create_model()
        
        assert isinstance(model, UNet), "Should return UNet instance"
        assert model.n_channels == 1, "Default should be single channel"
        assert model.n_classes == 1, "Default should be single output"
        
        # Test custom parameters
        model_custom = create_model(
            image_size=(128, 128),
            input_channels=2,
            output_channels=2
        )
        
        assert model_custom.n_channels == 2, "Should accept custom input channels"
        assert model_custom.n_classes == 2, "Should accept custom output channels"


def run_all_tests():
    """Run all tests manually"""
    test_classes = [
        TestUNetComponents(),
        TestUNetModel(), 
        TestMRIReconstructionDataset(),
        TestMRIReconstructionLoss(),
        TestModelIntegration()
    ]
    
    print("Running U-Net model tests...")
    
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
        print("\n🎉 All U-Net tests passed! Deep learning model is ready.")
    else:
        print("\n❌ Some tests failed. Please fix issues before proceeding.")
        sys.exit(1)