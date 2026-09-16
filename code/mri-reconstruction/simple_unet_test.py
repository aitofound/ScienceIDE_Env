"""
Simple test to verify U-Net architecture works
"""

import torch
import torch.nn as nn
import torch.nn.functional as F

class SimpleDoubleConv(nn.Module):
    def __init__(self, in_channels, out_channels):
        super().__init__()
        self.conv = nn.Sequential(
            nn.Conv2d(in_channels, out_channels, 3, padding=1),
            nn.BatchNorm2d(out_channels),
            nn.ReLU(inplace=True),
            nn.Conv2d(out_channels, out_channels, 3, padding=1),
            nn.BatchNorm2d(out_channels),
            nn.ReLU(inplace=True)
        )
    
    def forward(self, x):
        return self.conv(x)

class SimpleDown(nn.Module):
    def __init__(self, in_channels, out_channels):
        super().__init__()
        self.pool_conv = nn.Sequential(
            nn.MaxPool2d(2),
            SimpleDoubleConv(in_channels, out_channels)
        )
    
    def forward(self, x):
        return self.pool_conv(x)

class SimpleUp(nn.Module):
    def __init__(self, in_channels, out_channels):
        super().__init__()
        self.up = nn.Upsample(scale_factor=2, mode='bilinear', align_corners=True)
        # After concatenation: in_channels + out_channels -> out_channels
        self.conv = SimpleDoubleConv(in_channels + out_channels, out_channels)
    
    def forward(self, x1, x2):
        x1 = self.up(x1)
        
        # Handle size differences
        diffY = x2.size()[2] - x1.size()[2]
        diffX = x2.size()[3] - x1.size()[3]
        x1 = F.pad(x1, [diffX // 2, diffX - diffX // 2,
                        diffY // 2, diffY - diffY // 2])
        
        # Concatenate skip connection: [x2, x1]
        x = torch.cat([x2, x1], dim=1)
        return self.conv(x)

class SimpleUNet(nn.Module):
    def __init__(self):
        super().__init__()
        
        # Encoder
        self.inc = SimpleDoubleConv(1, 64)
        self.down1 = SimpleDown(64, 128)
        self.down2 = SimpleDown(128, 256)
        self.down3 = SimpleDown(256, 512)
        self.down4 = SimpleDown(512, 1024)
        
        # Decoder  
        self.up1 = SimpleUp(1024, 512)  # 1024 + 512 -> 512
        self.up2 = SimpleUp(512, 256)   # 512 + 256 -> 256
        self.up3 = SimpleUp(256, 128)   # 256 + 128 -> 128
        self.up4 = SimpleUp(128, 64)    # 128 + 64 -> 64
        
        # Output
        self.outc = nn.Conv2d(64, 1, 1)
    
    def forward(self, x):
        # Encoder
        x1 = self.inc(x)    # 64
        x2 = self.down1(x1) # 128
        x3 = self.down2(x2) # 256
        x4 = self.down3(x3) # 512
        x5 = self.down4(x4) # 1024
        
        # Decoder
        x = self.up1(x5, x4)  # 1024+512->512
        x = self.up2(x, x3)   # 512+256->256
        x = self.up3(x, x2)   # 256+128->128
        x = self.up4(x, x1)   # 128+64->64
        
        return self.outc(x)

def test_simple_unet():
    print("Testing simple U-Net...")
    model = SimpleUNet()
    x = torch.randn(1, 1, 64, 64)
    
    try:
        output = model(x)
        print(f"✅ Success! Input: {x.shape}, Output: {output.shape}")
        
        # Test with different sizes
        for size in [32, 128, 256]:
            test_x = torch.randn(1, 1, size, size)
            test_out = model(test_x)
            print(f"✅ Size {size}: Input {test_x.shape}, Output {test_out.shape}")
            
    except Exception as e:
        print(f"❌ Error: {str(e)}")

if __name__ == "__main__":
    test_simple_unet()