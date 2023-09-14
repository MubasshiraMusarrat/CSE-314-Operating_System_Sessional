#!/bin/bash

input_dir="$1"

mkdir -p "output_dir"

# Initialize a counter for renaming files
counter=0

# Loop through all files with execute permission in the input directory and its subdirectories
find "$input_dir" -type f -executable | while read -r file; do
  # Get the file owner
  owner=$(stat -c "%U" "$file")

  # Create the user-specific output directory if it doesn't exist
  user_output_dir="output_dir/$owner"
  mkdir -p "$user_output_dir"

  # Get the file extension
  ext="${file##*.}"

  # Get the last modification time of the file
  timestamp=$(date -r "$file" "+%s")

  # Create the new filename with a timestamp prefix
  new_filename="$counter _$(basename "$file")"
  
  # Copy the file to the user-specific output directory with the new filename
  cp "$file" "$user_output_dir/$new_filename"

  # Revoke execute permission on the copied file
  chmod -x "$user_output_dir/$new_filename"

  # Increment the counter for the next file
  ((counter++))
done
