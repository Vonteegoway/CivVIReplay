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
    ${PROJECT_SOURCE_DIR}/external/ImGui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/external/ImGui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/external/ImGui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/external/ImGui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/external/ImGui/backends/imgui_impl_opengl3.cpp
    ${PROJECT_SOURCE_DIR}/external/ImGui/backends/imgui_impl_glfw.cpp
)

# sqlite sources (shell.c defines main(), only harmless on Windows where the entry point is WinMain)
set(SQLITE_SOURCES ${PROJECT_SOURCE_DIR}/external/sqlite/sqlite3.c)
if(WIN32)
    list(APPEND SQLITE_SOURCES ${PROJECT_SOURCE_DIR}/external/sqlite/shell.c)
endif()

# -----------------------------
# External libraries
# -----------------------------
add_subdirectory(external/ZLib)

# -----------------------------
# Executable
# -----------------------------
set(PLATFORM_SOURCES)
if(WIN32)
    set(PLATFORM_SOURCES assets/resources/CivVIReplay.rc)
endif()

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES} ${SQLITE_SOURCES} ${PLATFORM_SOURCES})

if(WIN32)
    # Force proper C++ linkage and subsystem
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINKER_LANGUAGE CXX
        WIN32_EXECUTABLE OFF
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

# -----------------------------
# Include directories
# -----------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external
    ${PROJECT_SOURCE_DIR}/external/ImGui
    ${PROJECT_SOURCE_DIR}/external/ImGui/backends
    ${PROJECT_SOURCE_DIR}/external/sqlite
)

# Windows-only shims (dirent.h, bundled GLFW headers) must not shadow system headers elsewhere
if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include/win)
endif()

# -----------------------------
# Libraries
# -----------------------------
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        zlib
        ${PROJECT_SOURCE_DIR}/lib/glfw3_mt.lib
        opengl32
    )
else()
    find_package(glfw3 3.3 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        zlib
        glfw
        OpenGL::GL
        Threads::Threads
        ${CMAKE_DL_LIBS}
    )
endif()

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

# -----------------------------
# Install (Linux: ~/.local by default with -DCMAKE_INSTALL_PREFIX=$HOME/.local)
# -----------------------------
if(UNIX AND NOT APPLE)
    set(CIVREPLAY_DATADIR share/civvireplay)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CIVREPLAY_DATADIR})
    install(DIRECTORY assets/font assets/image DESTINATION ${CIVREPLAY_DATADIR})
    install(CODE "
        file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\")
        execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
            \"\${CMAKE_INSTALL_PREFIX}/${CIVREPLAY_DATADIR}/${PROJECT_NAME}\"
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/civvireplay\")
    ")
    configure_file(packaging/civvireplay.desktop.in ${CMAKE_BINARY_DIR}/civvireplay.desktop @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/civvireplay.desktop DESTINATION share/applications)
    install(FILES assets/image/icon/icon1.png DESTINATION share/icons/hicolor/128x128/apps RENAME civvireplay.png)
endif()
