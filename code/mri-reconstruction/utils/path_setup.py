"""
Project path setup utility
Add this to the top of any script to ensure proper module imports
"""

import sys
import os
from pathlib import Path

def setup_project_paths():
    """
    Add project root to Python path to enable imports from anywhere
    Call this at the top of any script that needs to import project modules
    """
    # Get the current file's directory
    current_file_path = Path(__file__).resolve()
    
    # Navigate up to find project root
    # Look for the mri-reconstruction-ai directory or specific marker files
    project_root = current_file_path
    
    while project_root.parent != project_root:
        # Check if this looks like our project root
        project_name = "mri-reconstruction-ai"
        if project_root.name == project_name:
            break
            
        # Alternative: look for specific directories that indicate project root
        expected_dirs = ['data', 'algorithms', 'evaluation', 'tests', 'pipeline']
        if all((project_root / d).exists() for d in expected_dirs[:3]):  # At least 3 of these
            break
            
        project_root = project_root.parent
    
    # If we went all the way to root without finding it, try a different approach
    if project_root.parent == project_root:
        # Start from current file and go up systematically
        current_dir = current_file_path.parent
        for _ in range(5):  # Don't go up more than 5 levels
            if current_dir.name == "mri-reconstruction-ai":
                project_root = current_dir
                break
            expected_dirs = ['data', 'algorithms', 'evaluation']
            if all((current_dir / d).exists() for d in expected_dirs):
                project_root = current_dir
                break
            current_dir = current_dir.parent
            if current_dir.parent == current_dir:  # Reached filesystem root
                break
    
    # Add project root to Python path if not already there
    project_root_str = str(project_root)
    if project_root_str not in sys.path:
        sys.path.insert(0, project_root_str)
        print(f"Added {project_root_str} to Python path")
    
    return project_root

def get_project_root():
    """
    Get the project root directory without modifying sys.path
    """
    current_file_path = Path(__file__).resolve()
    
    # Navigate up to find project root
    project_root = current_file_path
    
    while project_root.parent != project_root:
        if project_root.name == "mri-reconstruction-ai":
            return project_root
            
        expected_dirs = ['data', 'algorithms', 'evaluation']
        if all((project_root / d).exists() for d in expected_dirs):
            return project_root
            
        project_root = project_root.parent
    
    # Fallback: return current directory's parent hierarchy
    current_dir = current_file_path.parent
    for _ in range(5):
        if current_dir.name == "mri-reconstruction-ai":
            return current_dir
        expected_dirs = ['data', 'algorithms', 'evaluation']
        if all((current_dir / d).exists() for d in expected_dirs):
            return current_dir
        current_dir = current_dir.parent
        if current_dir.parent == current_dir:
            break
    
    # Last resort: assume we're in utils/ and go up one level
    return current_file_path.parent.parent

# Auto-setup when imported (optional)
if __name__ != "__main__":
    setup_project_paths()

# Test function
def test_imports():
    """Test that all main modules can be imported"""
    try:
        from data.data_generator import SyntheticMRIGenerator
        from algorithms.utils.kspace import KSpaceUtils
        from algorithms.classical.fista import FISTAReconstructor
        from algorithms.ai.unet import UNet
        from evaluation.metrics.reconstruction_metrics import ReconstructionMetrics
        print("✅ All imports successful!")
        return True
    except ImportError as e:
        print(f"❌ Import failed: {e}")
        return False

if __name__ == "__main__":
    print("Testing project path setup...")
    root = setup_project_paths()
    print(f"Project root: {root}")
    test_imports()