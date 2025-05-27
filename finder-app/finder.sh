#!/bin/sh

# Check if the required arguments are provided
if [ "$#" -lt 2 ]; then
    echo "Error: Insufficient arguments."
    echo "Usage: $0 <directory_path> <search_string>"
    exit 1
fi

filesdir="$1"
searchstr="$2"

# Check if the provided directory exists and is a directory
if [ ! -d "$filesdir" ]; then
    echo "Error: Directory '$filesdir' does not exist or is not a directory."
    exit 1
fi

# Count the number of files (regular files) in the directory and subdirectories
num_files=$(find "$filesdir" -type f | wc -l)

# Count the number of lines containing searchstr in those files
num_matching_lines=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

# Print the result
echo "The number of files are $num_files and the number of matching lines are $num_matching_lines"

exit 0