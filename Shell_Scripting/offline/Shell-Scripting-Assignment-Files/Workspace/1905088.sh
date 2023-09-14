#!/bin/bash

if [ "$#" -lt 4 ] || [ "$#" -gt 6 ]; then
  echo "Usage: organize.sh <submission_folder> <target_folder> <test_folder> <answer_folder> [-v] [-noexecute]"
  echo "-v: verbose"
  echo "-noexecute: do not execute code files"
  exit 1
fi

verbose=false
execute=true

target_folder="$PWD/$2"
test_folder="$PWD/$3"
answer_folder="$PWD/$4"

# Checking the number of arguments
if [ "$#" -gt 4 ]; then
  # Checking argument number 5
  case "$5" in
    "-v") verbose=true ;;
    "-nonexecute") execute=false ;;
    *) echo "Error: Invalid argument '$5'. Expected '-v' or '-nonexecute'."
      exit 1 ;;
  esac

  # Checking argument number 6
  if [ "$#" -gt 5 ]; then
    case "$6" in
      "-nonexecute") execute=false ;;
      *) echo "Error: Invalid argument '$6'. Expected '-nonexecute'."
        exit 1 ;;
    esac
  fi
fi

# Checking if the submission folder exists
if [ ! -d "$1" ]; then
  echo "Error: Submission folder '$1' does not exist."
  exit 1
fi

# Checking if the test folder exists
if [ ! -d "$3" ]; then
  echo "Error: Test folder '$3' does not exist."
  exit 1
fi

# Checking if the answer folder exists
if [ ! -d "$4" ]; then
  echo "Error: Answer folder '$4' does not exist."
  exit 1
fi

# Creating the main directory using the second argument
mkdir -p "$2"

# Creating subdirectories within the target directory
mkdir -p "$2/C"
mkdir -p "$2/Java"
mkdir -p "$2/Python"

# Creating result.csv
echo "student_id,type,matched,not_matched" > "$2/result.csv"

test_cases=0
# Verbose for test files
if [ "$verbose" = true ]; then
    test_cases=$(find "$3" -type f -name "*.txt" | wc -l)
    echo "Found $test_cases test cases"
fi

# goto submission folder
cd "$1"

# Looping through the submission files
for zip_file in *.zip; do
    # Extracting the student ID from the file name
    student_id="${zip_file%%.zip}"
    student_id="${student_id: -7}"

    # Verbose for organizing files
    if [ "$verbose" = true ]; then
      echo "Organizing files of $student_id"
    fi

    # Unzipping the folder
    unzip -q "$zip_file" -d "$student_id"

    ext_found=false
    extentions=("c" "java" "py")
    extension=""

    # Function to recursively search for files with specified extensions
    search_files() {
      for file in "$1"/*; do
        if [[ -f "$file" ]]; then
            ext="${file##*.}"
            if [[ " ${extentions[*]} " == *" $ext "* ]]; then
                ext_found=true
                extention="$ext"
                break
            fi
        elif [[ -d "$file" ]]; then
            search_files "$file"
        fi
      done
    }

    # Checking files in student_id folder
    search_files "$student_id"

    # If no file with specified extensions found, display error message
    if [[ "$ext_found" == false ]]; then
      echo "Error: No file found with .c, .java, or .py extension."
      exit 1
    fi

    directory=""
    new_filename=""
    type=""

    case "$extention" in
      c) type="C" ; directory="../$2/C/$student_id" ; new_filename="main.c" ;;
      java) type="Java" ; directory="../$2/Java/$student_id" ; new_filename="Main.java" ;;
      py) type="Python" ; directory="../$2/Python/$student_id" ; new_filename="main.py" ;;
      *) echo "Error: Invalid file extension."
        exit 1 ;;
    esac


    # Create the destination directory if it doesn't exist
    mkdir -p "$directory"

    # Copy the file to the destination directory and rename it
    cp "$file" "$directory/$new_filename"

    # Remove extracted files
    rm -rf "$student_id"

    # Save current path
    current_path=$(pwd)

    # Goto the target directory
    cd "$directory"

    # Compile the file
    case "$extention" in
      c) gcc "main.c" -o "main.out";;
      java) javac "Main.java";;
      py) # do nothing
        ;;
      *) echo "Error: Invalid file extension."
         exit 1 ;;
    esac

    # Execute the file
    if [ "$execute" = true ]; then
      if [ "$verbose" = true ]; then
        echo "Executing files of $student_id"
      fi

      # Looping through the test cases
      for test_file in "$test_folder"/*.txt; do

        # Extracting the test case number from the file name
        test_case_name="${test_file%%.txt}"
        test_case_name="${test_case_name##*/}"
        test_case_name=${test_case_name: -1}
    
        case "$extention" in
          c) ./main.out < "$test_file" > "out$test_case_name.txt" ;;
          java) java Main < "$test_file" > "out$test_case_name.txt" ;;
          py) python3 main.py < "$test_file" > "out$test_case_name.txt" ;;
          *) echo "Error: Invalid file extension."
             exit 1 ;;
        esac
      done

      # Comapare with answer files
      matched=0

      for ans_files in "$answer_folder"/*.txt; do

        # Extracting the test case number from the file name
        ans_case_name="${ans_files%%.txt}"
        ans_case_name="${ans_case_name##*/}"
        ans_case_name=${ans_case_name: -1}

        # Compare the output with the answer
        if diff "$ans_files" "out$ans_case_name.txt" >/dev/null; then
          matched=$((matched+1))
        fi
      done
      echo "$student_id,$type,$matched,$((test_cases-matched))" >> "$target_folder/result.csv"
    fi

  # Go back to the submission folder
  cd "$current_path"
done