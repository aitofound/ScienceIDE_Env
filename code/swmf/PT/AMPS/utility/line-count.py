#!/usr/bin/env python3
"""
Line Counter Script

This script counts the number of lines in files with specified extensions.
By default, it searches in the current directory for .h .cpp, .f90, .c, .for, .pl, and .py files.

Usage:
    python line_counter.py [options]

Options:
    -h, -help     Show this help message and exit
    -r            Recursively search through subdirectories
    -e EXT1,EXT2  Specify file extensions to search (comma-separated, without dots)
                  Example: -e py,txt,cpp
"""

import os
import sys


def print_help():
    """Print the help message."""
    print(__doc__)
    sys.exit(0)


def count_lines(file_path):
    """Count the number of lines in a file."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='replace') as file:
            return sum(1 for _ in file)
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return 0


def main():
    # Default file extensions to search for
    default_extensions = ['h', 'cpp', 'f90', 'c', 'for', 'pl', 'py']
    extensions = default_extensions
    recursive = False
    
    # Parse command line arguments
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg in ['-h', '-help']:
            print_help()
        elif arg == '-r':
            recursive = True
        elif arg == '-e' and i + 1 < len(sys.argv):
            extensions = sys.argv[i + 1].split(',')
            i += 1
        elif arg == '-e':
            print("Error: No extensions provided after -e flag.")
            print_help()
        else:
            print(f"Unknown argument: {arg}")
            print_help()
        i += 1
    
    # Convert extensions to lowercase and ensure they don't start with a dot
    extensions = [ext.lower().lstrip('.') for ext in extensions]
    
    # Initialize counters
    total_lines = 0
    total_files = 0
    visited_dirs = []
    
    # Function to process a directory
    def process_directory(directory):
        nonlocal total_lines, total_files, visited_dirs
        
        # Add directory to visited directories
        abs_dir = os.path.abspath(directory)
        visited_dirs.append(abs_dir)
        
        try:
            for item in os.listdir(directory):
                item_path = os.path.join(directory, item)
                
                # Process subdirectories if recursive flag is set
                if os.path.isdir(item_path) and recursive:
                    process_directory(item_path)
                
                # Process files with matching extensions
                elif os.path.isfile(item_path):
                    file_ext = os.path.splitext(item)[1].lstrip('.').lower()
                    if file_ext in extensions:
                        lines = count_lines(item_path)
                        total_lines += lines
                        total_files += 1
                        print(f"{item_path}: {lines} lines")
                        
        except PermissionError:
            print(f"Permission denied: {directory}")
        except Exception as e:
            print(f"Error processing directory {directory}: {e}")
    
    # Start processing from the current directory
    process_directory('.')
    
    # Print summary
    print("\nSummary:")
    print(f"Total files processed: {total_files}")
    print(f"Total lines counted: {total_lines}")
    print(f"\nFile extensions searched: {', '.join('.' + ext for ext in extensions)}")
    
    # Print visited directories
    print("\nDirectories visited:")
    for directory in visited_dirs:
        print(f"  {directory}")


if __name__ == "__main__":
    main()
