cmake_minimum_required(VERSION 3.10)
project(WaveSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Include directories
include_directories(include)
include_directories(libs/imgui)
include_directories(libs/imgui/backends)
include_directories(libs/glfw-3.4/include)

# Source files
set(SOURCES
    src/WaveSim.cpp
    src/glad.c
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp
    libs/imgui/backends/imgui_impl_opengl3.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    glfw
    OpenGL::GL
    "-framework Cocoa"
    "-framework IOKit" 
    "-framework CoreVideo"
    "-framework CoreFoundation"
)

# Set output name
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Wave Sim")
