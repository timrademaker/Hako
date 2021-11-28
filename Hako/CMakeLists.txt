# Minimum CMake version
cmake_minimum_required (VERSION 3.8)

# Set headers for Hako
set(HEADERS
    inc/Hako.h
    inc/IFile.h
    inc/IFileSerializer.h
    inc/HakoFile.h
)

# Set sources for Hako
set(SOURCES
    src/Hako.cpp
    src/IFile.cpp
    src/IFileSerializer.cpp
    src/HakoFile.cpp
)

if(HAKO_STANDALONE)
	add_compile_definitions("HAKO_STANDALONE")
endif(HAKO_STANDALOE)

# Create library for Hako
add_library(Hako EXCLUDE_FROM_ALL ${SOURCES} ${HEADERS})

# Add include directories
target_include_directories(Hako PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)
