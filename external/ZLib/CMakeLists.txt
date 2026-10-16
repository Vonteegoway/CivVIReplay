
# Collect all .cpp files in src/
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)


add_library(zlib STATIC ${SOURCES})

target_include_directories(zlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# gzlib.c uses lseek(); zlib only includes <unistd.h> when told it exists
if(NOT WIN32)
    target_compile_definitions(zlib PRIVATE HAVE_UNISTD_H)
endif()
