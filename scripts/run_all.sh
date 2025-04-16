#!/bin/bash

# Function to run tests with given configs
run_tests() {
    local configs=("$@")
    for config in "${configs[@]}"; do
        ./bin/GAME_APPLICATION -f=5 -c="$config"
    done
}

# Check if specific tests were requested
if [ $# -eq 0 ] || [[ "$*" == *"shader-test"* ]]; then
    echo -e "\nRunning shader-test:\n"
    configs=(
        "config/shader-test/test-0.jsonc"
        "config/shader-test/test-1.jsonc"
        "config/shader-test/test-2.jsonc"
        "config/shader-test/test-3.jsonc"
        "config/shader-test/test-4.jsonc"
        "config/shader-test/test-5.jsonc"
        "config/shader-test/test-6.jsonc"
        "config/shader-test/test-7.jsonc"
        "config/shader-test/test-8.jsonc"
        "config/shader-test/test-9.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"mesh-test"* ]]; then
    echo -e "\nRunning mesh-test:\n"
    configs=(
        # Default Mesh
        "config/mesh-test/default-0.jsonc"
        "config/mesh-test/default-1.jsonc"
        "config/mesh-test/default-2.jsonc"
        "config/mesh-test/default-3.jsonc"

        # Monkey Mesh
        "config/mesh-test/monkey-0.jsonc"
        "config/mesh-test/monkey-1.jsonc"
        "config/mesh-test/monkey-2.jsonc"
        "config/mesh-test/monkey-3.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"transform-test"* ]]; then
    echo -e "\nRunning transform-test:\n"
    configs=("config/transform-test/test-0.jsonc")
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"pipeline-test"* ]]; then
    echo -e "\nRunning pipeline-test:\n"
    configs=(
        # Face Culling
        "config/pipeline-test/fc-0.jsonc"
        "config/pipeline-test/fc-1.jsonc"
        "config/pipeline-test/fc-2.jsonc"
        "config/pipeline-test/fc-3.jsonc"

        # Depth Testing
        "config/pipeline-test/dt-0.jsonc"
        "config/pipeline-test/dt-1.jsonc"
        "config/pipeline-test/dt-2.jsonc"

        # Blending
        "config/pipeline-test/b-0.jsonc"
        "config/pipeline-test/b-1.jsonc"
        "config/pipeline-test/b-2.jsonc"
        "config/pipeline-test/b-3.jsonc"
        "config/pipeline-test/b-4.jsonc"

        # Mask
        "config/pipeline-test/cm-0.jsonc"
        "config/pipeline-test/dm-0.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"texture-test"* ]]; then
    echo -e "\nRunning texture-test:\n"
    configs=("config/texture-test/test-0.jsonc")
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"sampler-test"* ]]; then
    echo -e "\nRunning sampler-test:\n"
    configs=(
        "config/sampler-test/test-0.jsonc"
        "config/sampler-test/test-1.jsonc"
        "config/sampler-test/test-2.jsonc"
        "config/sampler-test/test-3.jsonc"
        "config/sampler-test/test-4.jsonc"
        "config/sampler-test/test-5.jsonc"
        "config/sampler-test/test-6.jsonc"
        "config/sampler-test/test-7.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"material-test"* ]]; then
    echo -e "\nRunning material-test:\n"
    configs=(
        "config/material-test/test-0.jsonc"
        "config/material-test/test-1.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"entity-test"* ]]; then
    echo -e "\nRunning entity-test:\n"
    configs=(
        "config/entity-test/test-0.jsonc"
        "config/entity-test/test-1.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"renderer-test"* ]]; then
    echo -e "\nRunning renderer-test:\n"
    configs=(
        "config/renderer-test/test-0.jsonc"
        "config/renderer-test/test-1.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"sky-test"* ]]; then
    echo -e "\nRunning sky-test:\n"
    configs=(
        "config/sky-test/test-0.jsonc"
        "config/sky-test/test-1.jsonc"
    )
    run_tests "${configs[@]}"
fi

if [ $# -eq 0 ] || [[ "$*" == *"postprocess-test"* ]]; then
    echo -e "\nRunning postprocess-test:\n"
    configs=(
        "config/postprocess-test/test-0.jsonc"
        "config/postprocess-test/test-1.jsonc"
        "config/postprocess-test/test-2.jsonc"
        "config/postprocess-test/test-3.jsonc"
    )
    run_tests "${configs[@]}"
fi 