#!/bin/bash

# Default values
RECURSIVE=false
NUM_FILES=10

# Usage function
usage() {
    echo "Usage: $0 [-r] [-n number_of_files] [directory]"
    echo ""
    echo "Options:"
    echo "  -r                : Perform a recursive search in all subdirectories"
    echo "  -n number_of_files: Specify the number of largest files to display (default is 10)"
    echo ""
    echo "Arguments:"
    echo "  directory         : Specify the directory to search (default is the current directory)"
    echo ""
    echo "Examples:"
    echo "  $0 -r -n 10 /path/to/directory   # Search recursively in /path/to/directory and display 10 largest files"
    echo "  $0 -n 5                         # Search in the current directory and display the 5 largest files"
    exit 1
}

# Parse command-line options
while getopts ":rn:" opt; do
    case "${opt}" in
        r)
            RECURSIVE=true
            ;;
        n)
            NUM_FILES=${OPTARG}
            # Validate that NUM_FILES is a number
            if ! [[ $NUM_FILES =~ ^[0-9]+$ ]]; then
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

# Function to convert bytes to human-readable format
convert_to_human_readable() {
    local bytes=$1
    if (( $(echo "$bytes >= 1099511627776" | bc -l) )); then
        echo "$(echo "scale=2; $bytes/1099511627776" | bc) TB"
    elif (( $(echo "$bytes >= 1073741824" | bc -l) )); then
        echo "$(echo "scale=2; $bytes/1073741824" | bc) GB"
    elif (( $(echo "$bytes >= 1048576" | bc -l) )); then
        echo "$(echo "scale=2; $bytes/1048576" | bc) MB"
    elif (( $(echo "$bytes >= 1024" | bc -l) )); then
        echo "$(echo "scale=2; $bytes/1024" | bc) KB"
    else
        echo "$bytes B"
    fi
}

# Find command based on whether recursive option is enabled
if $RECURSIVE; then
    FIND_CMD="find \"$DIRECTORY\" -type f"
else
    FIND_CMD="find \"$DIRECTORY\" -maxdepth 1 -type f"
fi

# Find and list the largest files, then calculate the total size
FILE_LIST=$(mktemp)

# Find files, sort them by size, and write to the temp file
eval "$FIND_CMD" -exec du -h {} + 2>/dev/null | sort -hr | head -n "$NUM_FILES" > "$FILE_LIST"

# Display the largest files
echo "Largest files:"
cat "$FILE_LIST"

# Calculate total size (handling human-readable formats like K, M, G)
total_size=0
while read -r size filename; do
    unit="${size: -1}"
    num="${size%?}"
    case $unit in
        K) total_size=$(echo "$total_size + $num * 1024" | bc) ;;
        M) total_size=$(echo "$total_size + $num * 1024 * 1024" | bc) ;;
        G) total_size=$(echo "$total_size + $num * 1024 * 1024 * 1024" | bc) ;;
        T) total_size=$(echo "$total_size + $num * 1024 * 1024 * 1024 * 1024" | bc) ;;
        *) total_size=$(echo "$total_size + ${size%?}" | bc) ;; # Assume bytes if no unit
    esac
done < "$FILE_LIST"

# Convert total size to human-readable format
total_size_human=$(convert_to_human_readable $total_size)

echo ""
echo "Total size of the largest files: $total_size_human"

# Clean up
rm "$FILE_LIST"

