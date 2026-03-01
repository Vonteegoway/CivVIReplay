# Collect all .cpp files in src/
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
# Create the library
add_library(civmodutilities ${SOURCES})

# Specify include directories
target_include_directories(civmodutilities
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui/
)

target_link_libraries(civmodutilities PRIVATE zlib)

# Specify required C++ standard (propagates to dependents)
target_compile_features(civmodutilities PUBLIC cxx_std_17)