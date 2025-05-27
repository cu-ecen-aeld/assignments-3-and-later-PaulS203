#!/bin/sh

# Check if the required arguments are provided
if [ "$#" -lt 2 ]; then
    echo "Error: Insufficient arguments."
    echo "Usage: $0 <file_path> <string_to_write>"
    exit 1
fi

writefile="$1"
writestr="$2"

# Extract the directory path
dirpath=$(dirname "$writefile")

# Create the directory if it doesn't exist
if ! mkdir -p "$dirpath"; then
    echo "Error: Failed to create directory '$dirpath'."
    exit 1
fi

# Write the string to the file, overwriting existing content
if ! echo "$writestr" > "$writefile"; then
    echo "Error: Could not write to file '$writefile'."
    exit 1
fi

exit 0
