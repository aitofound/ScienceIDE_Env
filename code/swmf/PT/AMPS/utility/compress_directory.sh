#!/bin/bash

# Function to display help message
function show_help() {
    echo "Usage: $0 <directory1> <directory2> ... [options]"
    echo
    echo "Options:"
    echo "  -move                        Move the resulting .tar.gz archives to the current directory"
    echo "  -h, --help                   Show this help message and exit"
    echo
    echo "Description:"
    echo "  This script compresses the specified directories into .tar.gz archives and deletes the original directories."
    echo "  By default, the archives will be kept in the same location as the original directories."
    echo "  If the -move option is specified, the archives will be moved to the current directory."
    echo
    echo "Example:"
    echo "  $0 /path/to/directory1 /path/to/directory2 -move"
    echo
}

# Check if any argument is provided
if [[ $# -eq 0 ]]; then
    echo "Error: No directory provided."
    show_help
    exit 1
fi

# Parse command-line options
MOVE=false
DIRECTORIES=()

while [[ "$1" != "" ]]; do
    case $1 in
        -h | --help )
            show_help
            exit 0
            ;;
        -move )
            MOVE=true
            ;;
        * )
            DIRECTORIES+=("$1")
            ;;
    esac
    shift
done

# Check if at least one directory is provided
if [[ ${#DIRECTORIES[@]} -eq 0 ]]; then
    echo "Error: No valid directory provided."
    show_help
    exit 1
fi

# Process each directory
for DIRECTORY in "${DIRECTORIES[@]}"; do
    # Check if the provided directory exists
    if [[ ! -d "$DIRECTORY" ]]; then
        echo "Error: Directory '$DIRECTORY' does not exist. Skipping."
        continue
    fi

    # Get the directory path and basename
    DIR_PATH=$(dirname "$DIRECTORY")
    DIR_NAME=$(basename "$DIRECTORY")

    # Compress the directory into a .tar.gz file in the same location as the original directory
    ARCHIVE_NAME="$DIR_PATH/$DIR_NAME.tar.gz"
    tar -czvf "$ARCHIVE_NAME" -C "$DIR_PATH" "$DIR_NAME"

    # Check if compression was successful
    if [[ $? -ne 0 ]]; then
        echo "Error: Failed to compress the directory '$DIRECTORY'. Skipping."
        continue
    fi

    # Remove the original directory
    rm -rf "$DIRECTORY"

    # Move the archive if the -move option is specified
    if [[ "$MOVE" == true ]]; then
        mv "$ARCHIVE_NAME" "$(pwd)"
        echo "Archive moved to $(pwd)/$DIR_NAME.tar.gz"
    else
        echo "Archive created at $ARCHIVE_NAME"
    fi
done
