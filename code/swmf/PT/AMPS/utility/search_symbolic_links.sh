#!/bin/bash

# Function to display help message
function show_help() {
    echo "Usage: $0 [-d directory] [options]"
    echo
    echo "Options:"
    echo "  -d, --directory <directory>   Specify the starting directory (default is current directory)"
    echo "  -h, --help                    Show this help message and exit"
    echo
    echo "Description:"
    echo "  This script searches for symbolic links in the specified directory and its subdirectories."
    echo "  It displays symbolic links along with details such as user, group, permissions, and time of creation."
    echo
}

# Default to current directory
SEARCH_DIR=$(pwd)

# Parse command-line options
while [[ "$1" != "" ]]; do
    case $1 in
        -d | --directory )       shift
                                 SEARCH_DIR=$1
                                 ;;
        -h | --help )            show_help
                                 exit 0
                                 ;;
        * )                      echo "Invalid option: $1"
                                 show_help
                                 exit 1
    esac
    shift
done

# Check if the provided directory exists
if [[ ! -d "$SEARCH_DIR" ]]; then
    echo "Error: Directory '$SEARCH_DIR' does not exist."
    exit 1
fi

# Search for symbolic links and show detailed information
echo "Searching for symbolic links in directory: $SEARCH_DIR"

# Find all symbolic links (-type l) and display detailed information with ls -l (permissions, user, group, time, target)
find "$SEARCH_DIR" -type l -exec ls -l {} +

# Print message if no symbolic links are found
if [ $? -ne 0 ]; then
    echo "No symbolic links found in $SEARCH_DIR."
fi

