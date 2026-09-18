#!/bin/bash

# Default values
RECURSIVE=false
NUM_DIRECTORIES=5

# Usage function
usage() {
    echo "Usage: $0 [-r] [-n number_of_directories] [directory]"
    echo ""
    echo "Options:"
    echo "  -r                        : Perform a recursive search in all subdirectories"
    echo "  -n number_of_directories   : Specify the number of largest directories to display (default is 5)"
    echo ""
    echo "Arguments:"
    echo "  directory                 : Specify the directory to calculate total size (default is the current directory)"
    echo ""
    echo "Examples:"
    echo "  $0 -r -n 10 /path/to/directory  # Calculate total size recursively and show 10 largest subdirectories"
    echo "  $0                             # Calculate total size of the current directory and show 5 largest subdirectories"
    exit 1
}

# Parse command-line options
while getopts ":rn:" opt; do
    case "${opt}" in
        r)
            RECURSIVE=true
            ;;
        n)
            NUM_DIRECTORIES=${OPTARG}
            # Validate that NUM_DIRECTORIES is a number
            if ! [[ $NUM_DIRECTORIES =~ ^[0-9]+$ ]]; then
                echo "Error: -n option requires a valid number"
                usage
            fi
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND -1))

# Set directory to search (default to current directory if none provided)
DIRECTORY=${1:-.}

# Check if the provided directory exists
if [[ ! -d $DIRECTORY ]]; then
    echo "Error: Directory $DIRECTORY does not exist"
    exit 1
fi

# Find command based on whether recursive option is enabled
if $RECURSIVE; then
    # Recursive search including all subdirectories
    TOTAL_SIZE=$(du -sh "$DIRECTORY" 2>/dev/null | awk '{print $1}')
    # Use du to list all directories, filter out the current directory itself, and sort by size
    LARGEST_DIRS=$(du -h "$DIRECTORY"/* 2>/dev/null | sort -hr | head -n "$NUM_DIRECTORIES")
else
    # Non-recursive search, calculating only the size of files in the current directory (excluding subdirectories)
    TOTAL_SIZE=$(find "$DIRECTORY" -maxdepth 1 -type f -exec du -ch {} + 2>/dev/null | grep total$ | awk '{print $1}')
    # List only the immediate subdirectories and files and sort them by size
    LARGEST_DIRS=$(du -h --max-depth=1 "$DIRECTORY" 2>/dev/null | sort -hr | grep -v "$DIRECTORY$" | head -n "$NUM_DIRECTORIES")
fi

# Output total size
echo "Total size of files in directory '$DIRECTORY': $TOTAL_SIZE"
echo ""
echo "Largest directories in '$DIRECTORY':"
echo "$LARGEST_DIRS"

