# Minimum CMake version
cmake_minimum_required (VERSION 3.8)

# Set headers for Hako
set(HEADERS
    inc/Hako.h
    inc/IFile.h
    inc/IFileSerializer.h
    inc/HakoFile.h
    inc/HakoPlatforms.h
    private/SerializerList.h
    private/MurmurHash3.h
)

# Set sources for Hako
set(SOURCES
    src/Hako.cpp
    src/IFile.cpp
    src/IFileSerializer.cpp
    src/HakoFile.cpp
    private/SerializerList.cpp
    private/MurmurHash3.cpp
)

if(HAKO_STANDALONE)
	add_compile_definitions("HAKO_STANDALONE")
endif(HAKO_STANDALONE)

if(HAKO_READ_OUTSIDE_OF_ARCHIVE)
    add_compile_definitions("HAKO_READ_OUTSIDE_OF_ARCHIVE")
endif(HAKO_READ_OUTSIDE_OF_ARCHIVE)

if(HAKO_NO_DYNAMIC_SERIALIZERS)
    add_compile_definitions("HAKO_NO_DYNAMIC_SERIALIZERS")
endif(HAKO_NO_DYNAMIC_SERIALIZERS)

# Create library for Hako
add_library(HakoLib EXCLUDE_FROM_ALL ${SOURCES} ${HEADERS})

# Add include directories
target_include_directories(HakoLib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)
target_include_directories(HakoLib PRIVATE ${CMAKE_CURRENT_LIST_DIR}/private)
