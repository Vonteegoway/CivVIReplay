cmake_minimum_required(VERSION 3.16)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
    # std::filesystem needs 10.15+; must be set before project()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

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

# Commit shown by --selftest, so a tester's report can be matched to a build
find_package(Git QUIET)
set(CIVREPLAY_GIT_COMMIT "unknown")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE CIVREPLAY_GIT_COMMIT_OUT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(CIVREPLAY_GIT_COMMIT_OUT)
        set(CIVREPLAY_GIT_COMMIT ${CIVREPLAY_GIT_COMMIT_OUT})
    endif()
endif()
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/SelfTest.cpp PROPERTIES
    COMPILE_DEFINITIONS CIVREPLAY_GIT_COMMIT="${CIVREPLAY_GIT_COMMIT}")

if(WIN32)
    # Force proper C++ linkage and subsystem
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINKER_LANGUAGE CXX
        WIN32_EXECUTABLE OFF
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

if(APPLE)
    # Build CivVIReplay.app; assets go to Contents/Resources (see Platform::assetPath)
    set(CIVREPLAY_ICNS ${CMAKE_BINARY_DIR}/CivVIReplay.icns)
    add_custom_command(OUTPUT ${CIVREPLAY_ICNS}
        COMMAND sh ${PROJECT_SOURCE_DIR}/packaging/make-icns.sh
                ${PROJECT_SOURCE_DIR}/assets/image/icon/icon1.png ${CIVREPLAY_ICNS}
        DEPENDS ${PROJECT_SOURCE_DIR}/assets/image/icon/icon1.png ${PROJECT_SOURCE_DIR}/packaging/make-icns.sh
        COMMENT "Generating CivVIReplay.icns")
    target_sources(${PROJECT_NAME} PRIVATE ${CIVREPLAY_ICNS})
    set_source_files_properties(${CIVREPLAY_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_NAME "Civ VI Replay"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.keride.civvireplay"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE CivVIReplay.icns
    )
    # OpenGL is deprecated on macOS but still available
    target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
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
    # macOS has no system GLFW: build it from source by default so the app is self-contained
    if(APPLE)
        set(CIVREPLAY_BUNDLED_GLFW_DEFAULT ON)
    else()
        set(CIVREPLAY_BUNDLED_GLFW_DEFAULT OFF)
    endif()
    option(CIVREPLAY_BUNDLED_GLFW "Build GLFW from source instead of using the system library" ${CIVREPLAY_BUNDLED_GLFW_DEFAULT})
    if(CIVREPLAY_BUNDLED_GLFW)
        include(FetchContent)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 # tag 3.4
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(glfw)
    else()
        find_package(glfw3 3.3 REQUIRED)
    endif()
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
if(APPLE)
    set(ASSET_DEST_DIR "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources")
else()
    set(ASSET_DEST_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
endif()
foreach(FOLDER ${ASSET_FOLDERS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/${FOLDER}"
        "${ASSET_DEST_DIR}/${FOLDER}"
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
