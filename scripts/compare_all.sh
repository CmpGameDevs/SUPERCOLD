#!/bin/bash

failure=0

# Function to compare a test group
compare_test() {
    local requirement="$1"
    local tolerance="$2"
    local threshold="$3"
    shift 3
    local files=("$@")

    echo -e "\nComparing $requirement output:"
    ./scripts/compare-group.sh -r "$requirement" -f "${files[@]}" -t "$tolerance" -e "$threshold"
    failure=$((failure + $?))
}

# Check if specific tests were requested
if [ $# -eq 0 ] || [[ "$*" == *"shader-test"* ]]; then
    files=(
        "test-0.png"
        "test-1.png"
        "test-2.png"
        "test-3.png"
        "test-4.png"
        "test-5.png"
        "test-6.png"
        "test-7.png"
        "test-8.png"
        "test-9.png"
    )
    compare_test "shader-test" "0.01" "0" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"mesh-test"* ]]; then
    files=(
        "default-0.png"
        "default-1.png"
        "default-2.png"
        "default-3.png"
        "monkey-0.png"
        "monkey-1.png"
        "monkey-2.png"
        "monkey-3.png"
    )
    compare_test "mesh-test" "0.01" "0" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"transform-test"* ]]; then
    files=("test-0.png")
    compare_test "transform-test" "0.01" "0" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"pipeline-test"* ]]; then
    files=(
        "fc-0.png"
        "fc-1.png"
        "fc-2.png"
        "fc-3.png"
        "dt-0.png"
        "dt-1.png"
        "dt-2.png"
        "b-0.png"
        "b-1.png"
        "b-2.png"
        "b-3.png"
        "b-4.png"
        "cm-0.png"
        "dm-0.png"
    )
    compare_test "pipeline-test" "0.01" "64" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"texture-test"* ]]; then
    files=("test-0.png")
    compare_test "texture-test" "0.01" "0" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"sampler-test"* ]]; then
    files=(
        "test-0.png"
        "test-1.png"
        "test-2.png"
        "test-3.png"
        "test-4.png"
        "test-5.png"
        "test-6.png"
        "test-7.png"
    )
    compare_test "sampler-test" "0.01" "0" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"material-test"* ]]; then
    files=(
        "test-0.png"
        "test-1.png"
    )
    compare_test "material-test" "0.02" "64" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"entity-test"* ]]; then
    files=(
        "test-0.png"
        "test-1.png"
    )
    compare_test "entity-test" "0.04" "64" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"renderer-test"* ]]; then
    files=(
        "test-0.png"
        "test-1.png"
    )
    compare_test "renderer-test" "0.04" "64" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"sky-test"* ]]; then
    files=(
        "test-0.png"
        "test-1.png"
    )
    compare_test "sky-test" "0.04" "64" "${files[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"postprocess-test"* ]]; then
    files=(
        "test-0.png"
        "test-1.png"
        "test-2.png"
        "test-3.png"
    )
    compare_test "postprocess-test" "0.04" "64" "${files[@]}"
fi

echo -e "\nOverall Results"
if [ $failure -eq 0 ]; then
    echo "SUCCESS: All outputs are correct"
else
    if [ $failure -eq 1 ]; then
        echo "FAILURE: $failure output is incorrect"
    else
        echo "FAILURE: $failure outputs are incorrect"
    fi
fi

exit $failure 