"""
U-Net model for MRI reconstruction
Deep learning approach to learn mapping from undersampled to fully sampled images
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np
from typing import Tuple, Optional


class DoubleConv(nn.Module):
    """Double convolution block (conv -> bn -> relu -> conv -> bn -> relu)"""
    
    def __init__(self, in_channels: int, out_channels: int, 
                 dropout_rate: float = 0.1):
        super(DoubleConv, self).__init__()
        self.double_conv = nn.Sequential(
            nn.Conv2d(in_channels, out_channels, kernel_size=3, padding=1),
            nn.BatchNorm2d(out_channels),
            nn.ReLU(inplace=True),
            nn.Dropout2d(dropout_rate),
            nn.Conv2d(out_channels, out_channels, kernel_size=3, padding=1),
            nn.BatchNorm2d(out_channels),
            nn.ReLU(inplace=True)
        )
    
    def forward(self, x):
        return self.double_conv(x)


class Down(nn.Module):
    """Downsampling with maxpool then double conv"""
    
    def __init__(self, in_channels: int, out_channels: int, 
                 dropout_rate: float = 0.1):
        super(Down, self).__init__()
        self.maxpool_conv = nn.Sequential(
            nn.MaxPool2d(2),
            DoubleConv(in_channels, out_channels, dropout_rate)
        )
    
    def forward(self, x):
        return self.maxpool_conv(x)


class Up(nn.Module):
    """Upsampling then double conv"""
    
    def __init__(self, in_channels: int, out_channels: int, 
                 bilinear: bool = True, dropout_rate: float = 0.1):
        super(Up, self).__init__()
        
        # Always use bilinear upsampling for simplicity (like the working version)
        self.up = nn.Upsample(scale_factor=2, mode='bilinear', align_corners=True)
        
        # After upsampling + concatenation with skip: in_channels + out_channels
        self.conv = DoubleConv(in_channels + out_channels, out_channels, dropout_rate)
    
    def forward(self, x1, x2):
        """
        Forward pass for Up block
        
        Args:
            x1: Lower resolution feature map (to be upsampled)
            x2: Higher resolution feature map (skip connection)
        """
        # Upsample x1
        x1 = self.up(x1)
        
        # Handle potential size mismatch between x1 and x2
        diffY = x2.size()[2] - x1.size()[2]
        diffX = x2.size()[3] - x1.size()[3]
        
        if diffX != 0 or diffY != 0:
            x1 = F.pad(x1, [diffX // 2, diffX - diffX // 2,
                           diffY // 2, diffY - diffY // 2])
        
        # Concatenate: [batch, x2_channels + x1_channels, height, width]
        x = torch.cat([x2, x1], dim=1)
        
        # Apply convolution
        return self.conv(x)


class UNet(nn.Module):
    """
    U-Net architecture for MRI reconstruction
    Takes undersampled MRI images and reconstructs fully sampled versions
    """
    
    def __init__(self, 
                 n_channels: int = 1, 
                 n_classes: int = 1,
                 features: Tuple[int, ...] = (64, 128, 256, 512, 1024),
                 bilinear: bool = True,
                 dropout_rate: float = 0.1):
        """
        Initialize U-Net
        
        Args:
            n_channels: Number of input channels (1 for magnitude, 2 for complex)
            n_classes: Number of output channels (1 for magnitude reconstruction)
            features: Number of features at each level
            bilinear: Use bilinear upsampling instead of transpose convolutions
            dropout_rate: Dropout rate for regularization
        """
        super(UNet, self).__init__()
        self.n_channels = n_channels
        self.n_classes = n_classes
        self.bilinear = bilinear
        
        # Initial convolution
        self.inc = DoubleConv(n_channels, features[0], dropout_rate)
        
        # Encoder (downsampling path)
        self.down1 = Down(features[0], features[1], dropout_rate)
        self.down2 = Down(features[1], features[2], dropout_rate)
        self.down3 = Down(features[2], features[3], dropout_rate)
        
        # Bottleneck
        self.down4 = Down(features[3], features[4], dropout_rate)
        
        # Decoder (upsampling path)  
        # Each Up block: upsampled_features + skip_connection -> output
        self.up1 = Up(features[4], features[3], bilinear, dropout_rate)  # 1024+512->512
        self.up2 = Up(features[3], features[2], bilinear, dropout_rate)  # 512+256->256
        self.up3 = Up(features[2], features[1], bilinear, dropout_rate)  # 256+128->128
        self.up4 = Up(features[1], features[0], bilinear, dropout_rate)  # 128+64->64
        
        # Output layer
        self.outc = nn.Conv2d(features[0], n_classes, kernel_size=1)
        
        # Initialize weights
        self._initialize_weights()
    
    def _initialize_weights(self):
        """Initialize network weights using He initialization"""
        for m in self.modules():
            if isinstance(m, nn.Conv2d):
                nn.init.kaiming_normal_(m.weight, mode='fan_out', 
                                      nonlinearity='relu')
                if m.bias is not None:
                    nn.init.constant_(m.bias, 0)
            elif isinstance(m, nn.BatchNorm2d):
                nn.init.constant_(m.weight, 1)
                nn.init.constant_(m.bias, 0)
    
    def forward(self, x):
        """Forward pass through the network"""
        # Encoder path
        x1 = self.inc(x)
        x2 = self.down1(x1)
        x3 = self.down2(x2)
        x4 = self.down3(x3)
        x5 = self.down4(x4)
        
        # Decoder path with skip connections
        x = self.up1(x5, x4)  # Pass both bottleneck and skip connection
        x = self.up2(x, x3)   # Pass result and skip connection
        x = self.up3(x, x2)   # Pass result and skip connection
        x = self.up4(x, x1)   # Pass result and skip connection
        
        # Output
        logits = self.outc(x)
        return logits
    
    def count_parameters(self):
        """Count the number of trainable parameters"""
        return sum(p.numel() for p in self.parameters() if p.requires_grad)


class MRIReconstructionDataset(torch.utils.data.Dataset):
    """Dataset for MRI reconstruction training"""
    
    def __init__(self, 
                 ground_truth_images: np.ndarray,
                 undersampled_images: np.ndarray,
                 transform: Optional[callable] = None):
        """
        Initialize dataset
        
        Args:
            ground_truth_images: Fully sampled ground truth images
            undersampled_images: Undersampled input images
            transform: Optional data transformations
        """
        self.ground_truth = ground_truth_images
        self.undersampled = undersampled_images
        self.transform = transform
        
        assert len(ground_truth_images) == len(undersampled_images), \
            "Ground truth and undersampled data must have same length"
    
    def __len__(self):
        return len(self.ground_truth)
    
    def __getitem__(self, idx):
        gt_image = self.ground_truth[idx]
        us_image = self.undersampled[idx]
        
        # Add channel dimension if needed
        if gt_image.ndim == 2:
            gt_image = gt_image[np.newaxis, ...]
        if us_image.ndim == 2:
            us_image = us_image[np.newaxis, ...]
        
        # Convert to tensors
        gt_tensor = torch.from_numpy(gt_image.astype(np.float32))
        us_tensor = torch.from_numpy(us_image.astype(np.float32))
        
        if self.transform:
            gt_tensor = self.transform(gt_tensor)
            us_tensor = self.transform(us_tensor)
        
        return us_tensor, gt_tensor


class MRIReconstructionLoss(nn.Module):
    """Combined loss function for MRI reconstruction"""
    
    def __init__(self, 
                 mse_weight: float = 1.0,
                 ssim_weight: float = 0.1,
                 l1_weight: float = 0.1):
        """
        Initialize loss function
        
        Args:
            mse_weight: Weight for MSE loss
            ssim_weight: Weight for SSIM loss (structural similarity)
            l1_weight: Weight for L1 loss (sparsity)
        """
        super(MRIReconstructionLoss, self).__init__()
        self.mse_weight = mse_weight
        self.ssim_weight = ssim_weight
        self.l1_weight = l1_weight
        
        self.mse_loss = nn.MSELoss()
        self.l1_loss = nn.L1Loss()
    
    def _ssim_loss(self, pred, target, window_size=11, sigma=1.5):
        """
        Simplified SSIM loss calculation
        """
        C1 = 0.01 ** 2
        C2 = 0.03 ** 2
        
        mu1 = F.avg_pool2d(pred, window_size, 1, padding=window_size//2)
        mu2 = F.avg_pool2d(target, window_size, 1, padding=window_size//2)
        
        mu1_sq = mu1.pow(2)
        mu2_sq = mu2.pow(2)
        mu1_mu2 = mu1 * mu2
        
        sigma1_sq = F.avg_pool2d(pred * pred, window_size, 1, 
                               padding=window_size//2) - mu1_sq
        sigma2_sq = F.avg_pool2d(target * target, window_size, 1, 
                               padding=window_size//2) - mu2_sq
        sigma12 = F.avg_pool2d(pred * target, window_size, 1, 
                             padding=window_size//2) - mu1_mu2
        
        ssim_map = ((2 * mu1_mu2 + C1) * (2 * sigma12 + C2)) / \
                  ((mu1_sq + mu2_sq + C1) * (sigma1_sq + sigma2_sq + C2))
        
        return 1 - ssim_map.mean()
    
    def forward(self, pred, target):
        """Compute combined loss"""
        mse = self.mse_loss(pred, target)
        l1 = self.l1_loss(pred, target)
        ssim = self._ssim_loss(pred, target)
        
        total_loss = (self.mse_weight * mse + 
                     self.l1_weight * l1 + 
                     self.ssim_weight * ssim)
        
        return total_loss, {
            'mse': mse.item(),
            'l1': l1.item(),
            'ssim': ssim.item(),
            'total': total_loss.item()
        }


def create_model(image_size: Tuple[int, int] = (256, 256),
                input_channels: int = 1,
                output_channels: int = 1) -> UNet:
    """
    Create and return a U-Net model for MRI reconstruction
    
    Args:
        image_size: Expected input image size
        input_channels: Number of input channels
        output_channels: Number of output channels
        
    Returns:
        Initialized U-Net model
    """
    model = UNet(
        n_channels=input_channels,
        n_classes=output_channels,
        features=(64, 128, 256, 512, 1024),
        bilinear=True,
        dropout_rate=0.1
    )
    
    return model


def main():
    """Test the U-Net model"""
    print("Testing U-Net model for MRI reconstruction...")
    
    # Create model
    model = create_model(image_size=(128, 128))
    print(f"Model created with {model.count_parameters():,} parameters")
    
    # Test forward pass
    batch_size = 2
    test_input = torch.randn(batch_size, 1, 128, 128)
    
    model.eval()
    with torch.no_grad():
        output = model(test_input)
    
    print(f"Input shape: {test_input.shape}")
    print(f"Output shape: {output.shape}")
    
    # Test loss function
    loss_fn = MRIReconstructionLoss()
    target = torch.randn_like(output)
    
    total_loss, loss_dict = loss_fn(output, target)
    print(f"Loss components: {loss_dict}")
    
    # Test dataset creation
    dummy_gt = np.random.rand(10, 128, 128)
    dummy_us = np.random.rand(10, 128, 128)
    
    dataset = MRIReconstructionDataset(dummy_gt, dummy_us)
    print(f"Dataset created with {len(dataset)} samples")
    
    # Test data loading
    dataloader = torch.utils.data.DataLoader(dataset, batch_size=2, shuffle=True)
    for batch_idx, (inputs, targets) in enumerate(dataloader):
        print(f"Batch {batch_idx}: Input {inputs.shape}, Target {targets.shape}")
        if batch_idx == 0:  # Only show first batch
            break
    
    print("U-Net model working correctly!")


if __name__ == "__main__":
    main()