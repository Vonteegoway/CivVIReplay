cmake_minimum_required(VERSION 3.16)

# -----------------------------
# Project
# -----------------------------
project(CivVIReplay VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force static or multi-threaded CRT (adjust to /MT or /MD if needed)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
endif()

# -----------------------------
# Sources
# -----------------------------
# All project source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

# ImGui sources
set(IMGUI_SOURCES
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
)

# -----------------------------
# External libraries
# -----------------------------
add_subdirectory(external/zlib)

# -----------------------------
# Internal library
# -----------------------------
add_subdirectory(civmodutilities)

# -----------------------------
# Executable
# -----------------------------
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES} assets/resources/CivVIReplay.rc)

# Force proper C++ linkage and subsystem
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINKER_LANGUAGE CXX
    WIN32_EXECUTABLE OFF
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)

# -----------------------------
# Include directories
# -----------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external
    ${PROJECT_SOURCE_DIR}/external/imgui
    ${PROJECT_SOURCE_DIR}/external/imgui/backends
)

# -----------------------------
# Libraries
# -----------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    civmodutilities
    zlib
    ${PROJECT_SOURCE_DIR}/lib/glfw3_mt.lib
    opengl32
)

# -----------------------------
# Compile definitions
# -----------------------------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
)

# -----------------------------
# Copy runtime assets
# -----------------------------
set(ASSET_FOLDERS font image shader)
foreach(FOLDER ${ASSET_FOLDERS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/${FOLDER}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${FOLDER}"
        COMMENT "Copying ${FOLDER} to output directory"
    )
endforeach()