cmake_minimum_required(VERSION 3.14)
project(SUPERCOLD VERSION 1.1.0)

# ==============================================================================
# Global build settings
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/bin)

# ==============================================================================
# Platform-specific settings
# ==============================================================================
if(NOT WIN32)
    set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE PATH "" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/g++" CACHE PATH "" FORCE)
endif()

# ==============================================================================
# Third-party libraries configuration
# ==============================================================================

# Include FetchContent for downloading dependencies
include(FetchContent)

# --- Find system/vcpkg packages ---
find_package(OpenAL CONFIG REQUIRED)
find_package(TBB REQUIRED)
find_package(assimp CONFIG REQUIRED)

# --- GLFW options ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_USE_HYBRID_HPG ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

# --- Bullet Physics options ---
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_ENET OFF CACHE BOOL "" FORCE)
set(BUILD_CLSOCKET OFF CACHE BOOL "" FORCE)

# Shared libs setting (might affect Bullet3 if it respects it, or other FetchContent libs)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ==============================================================================
# Third-party libraries inclusion
# ==============================================================================

# Add GLFW from local source
add_subdirectory(vendor/glfw)

# Add local libraries
add_subdirectory(vendor/bullet3)
add_subdirectory(vendor/freetype-2.13.3 vendor/freetype-2.13.3/build)

# ==============================================================================
# Include directories
# ==============================================================================
include_directories(
    source/common
    source/game
    vendor/glfw/include
    vendor/glad/include
    vendor/glm
    vendor/imgui
    vendor/utils
    vendor/bullet3/src
    vendor/freetype-2.13.3/include
)

# ==============================================================================
# Source files grouping
# ==============================================================================

# --- Vendor source files ---
set(GLAD_SOURCE vendor/glad/src/gl.c)

set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_impl/imgui_impl_glfw.cpp
    vendor/imgui/imgui_impl/imgui_impl_opengl3.cpp
)

set(VENDOR_SOURCES ${GLAD_SOURCE} ${IMGUI_SOURCES})

# --- Common source files ---
set(COMMON_SOURCES
    # Core
    source/common/application.hpp
    source/common/application.cpp
    source/common/asset-loader.cpp
    source/common/asset-loader.hpp
    source/common/deserialize-utils.hpp
    
    # Input
    source/common/input/keyboard.hpp
    source/common/input/mouse.hpp
    
    # Shader
    source/common/shader/shader.hpp
    source/common/shader/shader.cpp
    
    # Mesh
    source/common/mesh/vertex.hpp
    source/common/mesh/mesh.hpp
    source/common/mesh/mesh-utils.hpp
    source/common/mesh/mesh-utils.cpp
    
    # Audio
    source/common/audio/audio-buffer.hpp
    source/common/audio/audio-utils.hpp
    source/common/audio/audio-utils.cpp
    
    # Texture
    source/common/texture/sampler.hpp
    source/common/texture/sampler.cpp
    source/common/texture/texture2d.hpp
    source/common/texture/texture-utils.hpp
    source/common/texture/texture-utils.cpp
    source/common/texture/screenshot.hpp
    source/common/texture/screenshot.cpp
    source/common/texture/cubemap-texture.hpp
    source/common/texture/cubemap-texture.cpp
    source/common/texture/texture-unit.hpp
    
    # Material
    source/common/material/pipeline-state.hpp
    source/common/material/pipeline-state.cpp
    source/common/material/material.hpp
    source/common/material/material.cpp
    
    # IBL
    source/common/ibl/cubemap-buffer.hpp
    source/common/ibl/cubemap-buffer.cpp
    source/common/ibl/cubemap.hpp
    source/common/ibl/cubemap.cpp
    source/common/ibl/hdr-system.hpp
    source/common/ibl/hdr-system.cpp
    source/common/ibl/bloom-buffer.hpp
    source/common/ibl/bloom-buffer.cpp
    source/common/ibl/fullscreenquad.hpp
    source/common/ibl/fullscreenquad.cpp
    source/common/ibl/postprocess.hpp
    source/common/ibl/postprocess.cpp
    
    # ECS Core
    source/common/ecs/component.hpp
    source/common/ecs/transform.hpp
    source/common/ecs/transform.cpp
    source/common/ecs/entity.hpp
    source/common/ecs/entity.cpp
    source/common/ecs/world.hpp
    source/common/ecs/world.cpp
    source/common/ecs/lighting.hpp
    source/common/ecs/lighting.cpp
    
    # Components
    source/common/components/camera.hpp
    source/common/components/camera.cpp
    source/common/components/mesh-renderer.hpp
    source/common/components/mesh-renderer.cpp
    source/common/components/free-camera-controller.hpp
    source/common/components/free-camera-controller.cpp
    source/common/components/fps-controller.hpp
    source/common/components/fps-controller.cpp
    source/common/components/enemy-controller.hpp
    source/common/components/enemy-controller.cpp
    source/common/components/movement.hpp
    source/common/components/movement.cpp
    source/common/components/collision.cpp # Note: Repeated, assuming typo and meant .hpp and .cpp
    source/common/components/collision.hpp
    source/common/components/component-deserializer.hpp
    source/common/components/model-renderer.hpp
    source/common/components/model-renderer.cpp
    source/common/components/audio.hpp
    source/common/components/audio.cpp
    source/common/components/weapon.hpp
    source/common/components/weapon.cpp
    source/common/components/crosshair.hpp
    source/common/components/crosshair.cpp
    
    # Model
    source/common/model/model.hpp
    source/common/model/model.cpp
    
    # Systems
    source/common/systems/audio-system.hpp
    source/common/systems/audio-system.cpp
    source/common/systems/collision-system.hpp
    source/common/systems/collision-system.cpp
    source/common/systems/weapons-system.hpp
    source/common/systems/weapons-system.cpp
    source/common/systems/enemy-system.hpp
    source/common/systems/enemy-system.cpp
    source/common/systems/forward-renderer.hpp
    source/common/systems/forward-renderer.cpp
    source/common/systems/free-camera-controller.hpp
    source/common/systems/fps-controller.hpp
    source/common/systems/movement.hpp
)

# --- Game state files ---
set(STATES_SOURCES
    source/states/play-state.hpp
    source/states/menu-state.hpp
    source/states/shader-test-state.hpp
    source/states/mesh-test-state.hpp
    source/states/transform-test-state.hpp
    source/states/pipeline-test-state.hpp
    source/states/texture-test-state.hpp
    source/states/sampler-test-state.hpp
    source/states/material-test-state.hpp
    source/states/entity-test-state.hpp
    source/states/renderer-test-state.hpp
    source/states/light-test-state.hpp
    source/states/physics-test-state.hpp
)

# ==============================================================================
# Target definition
# ==============================================================================
add_executable(SUPERCOLD source/main.cpp ${STATES_SOURCES} ${COMMON_SOURCES} ${VENDOR_SOURCES})

target_link_libraries(SUPERCOLD
    PRIVATE
        glfw 
        BulletDynamics 
        BulletCollision 
        LinearMath
        freetype
        OpenAL::OpenAL
        assimp::assimp 
        TBB::tbb
)

