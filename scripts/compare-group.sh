#!/bin/bash

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--requirement)
            requirement="$2"
            shift 2
            ;;
        -f|--files)
            shift
            files=()
            while [[ $# -gt 0 && ! $1 =~ ^- ]]; do
                files+=("$1")
                shift
            done
            ;;
        -t|--tolerance)
            tolerance="$2"
            shift 2
            ;;
        -e|--threshold)
            threshold="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check required arguments
if [ -z "$requirement" ] || [ -z "$files" ] || [ -z "$tolerance" ] || [ -z "$threshold" ]; then
    echo "Usage: $0 -r|--requirement <requirement> -f|--files <files...> -t|--tolerance <tolerance> -e|--threshold <threshold>"
    exit 1
fi

expected="expected/$requirement"
output="screenshots/$requirement"
errors="errors/$requirement"

# Create errors directory if it doesn't exist
mkdir -p "$errors"

success=0

# Compare each file
for file in "${files[@]}"; do
    echo "Testing $file ..."
    ./scripts/imgcmp "$expected/$file" "$output/$file" -o "$errors/$file" -t "$tolerance" -e "$threshold"
    if [ $? -eq 0 ]; then
        ((success++))
    fi
done

total=${#files[@]}
echo -e "\nMatches: $success/$total"
if [ $success -eq $total ]; then
    echo "SUCCESS: All outputs are correct"
    exit 0
else
    failure=$((total - success))
    if [ $failure -eq 1 ]; then
        echo "FAILURE: $failure output is incorrect"
    else
        echo "FAILURE: $failure outputs are incorrect"
    fi
    exit $failure
fi 