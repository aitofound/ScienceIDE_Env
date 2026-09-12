#!/bin/bash

# Function to display help message
function show_help() {
    echo "Usage: $0 [-d directory] [-n number] [-total] [-h]"
    echo
    echo "Options:"
    echo "  -d, --directory <directory>   Specify the starting directory (default is the current directory)"
    echo "  -n, --number <number>         Specify the number of top directories to output (default is 10)"
    echo "  -total                        Sort output based on the total number of files (including subdirectories)"
    echo "  -h, --help                    Show this help message and exit"
    echo
    echo "Description:"
    echo "  This script recursively determines the subdirectories with the largest number of files."
    echo "  It outputs the subdirectory name, the number of files it contains, the total number of files including all subdirectories, and the total size of the files."
    echo
    echo "Example:"
    echo "  $0 -d /path/to/start -n 5 -total"
    echo
}

# Default to current directory and top 10 directories
START_DIR=$(pwd)
TOP_N=10
SORT_BY_TOTAL=0

# Parse command-line options
while [[ "$1" != "" ]]; do
    case $1 in
        -d | --directory )   shift
                             START_DIR=$1
                             ;;
        -n | --number )      shift
                             TOP_N=$1
                             ;;
        -total )             SORT_BY_TOTAL=1
                             ;;
        -h | --help )        show_help
                             exit 0
                             ;;
        * )                  echo "Invalid option: $1"
                             show_help
                             exit 1
    esac
    shift
done

# Check if the provided directory exists
if [[ ! -d "$START_DIR" ]]; then
    echo "Error: Directory '$START_DIR' does not exist."
    exit 1
fi

# Find all subdirectories and calculate the number of files and total size
find "$START_DIR" -type d | while read dir; do
    # Count the number of files in the subdirectory (without files in subdirectories)
    file_count=$(find "$dir" -maxdepth 1 -type f | wc -l)
    
    # Count the total number of files including subdirectories
    total_file_count=$(find "$dir" -type f | wc -l)
    
    # Calculate the total size of the files in the subdirectory and its subdirectories
    total_size=$(du -sh "$dir" 2>/dev/null | cut -f1)

    echo "$file_count $total_file_count $total_size $dir"
done | {
    if [[ $SORT_BY_TOTAL -eq 1 ]]; then
        sort -k2 -nr  # Sort by total file count (second column)
    else
        sort -k1 -nr  # Sort by direct file count (first column)
    fi
} | head -n "$TOP_N" | awk '{printf "%s files (direct), %s files (total) in %s (total size: %s)\n", $1, $2, $4, $3}'

