#!/bin/bash

# Default file extension is .dat
extension="dat"

# Function to display help
show_help() {
    echo "Usage: compress_files.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -r           Compress files in the current directory and all subdirectories (recursive)"
    echo "  -type <ext>  Specify the file extension to compress (default: dat)"
    echo "  -h, -help    Display this help message"
    echo ""
    echo "Example:"
    echo "  ./compress_files.sh -r -type txt"
    echo "  Compress all .txt files in the current directory and subdirectories."
    exit 0
}

# Function to compress files in the current directory
compress_current_dir() {
    for file in *.$extension; do
        if [ -f "$file" ]; then
            echo "Compressing $file..."
            gzip "$file"
        fi
    done
}

# Function to compress files in all subdirectories
compress_recursive() {
    find . -type f -name "*.$extension" | while read -r file; do
        echo "Compressing $file..."
        gzip "$file"
    done
}

# Parse command-line options
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -r) recursive=true ;;  # Enable recursive option
        -type) shift; extension="$1" ;;  # Set file extension
        -h | -help) show_help ;;  # Show help message
        *) echo "Unknown parameter passed: $1"; show_help ;;
    esac
    shift
done

# Check if recursive flag is set and call the appropriate function
if [ "$recursive" = true ]; then
    echo "Compressing *.$extension files in current directory and all subdirectories..."
    compress_recursive
else
    echo "Compressing *.$extension files in the current directory only..."
    compress_current_dir
fi

