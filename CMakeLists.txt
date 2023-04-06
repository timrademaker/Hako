# Minimum CMake version
cmake_minimum_required (VERSION 3.8)

# Create the project
project ("Hako" LANGUAGES CXX)

# Set the C++ language standard
set(CMAKE_CXX_STANDARD
	17
)
set(CMAKE_CXX_STANDARD_REQUIRED
	True
)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
	${CMAKE_CURRENT_LIST_DIR}/bin/${BUILD_PLATFORM}/${CMAKE_BUILD_TYPE}/bin
)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY 
	${CMAKE_CURRENT_LIST_DIR}/bin/${BUILD_PLATFORM}/${CMAKE_BUILD_TYPE}/lib
)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
	${CMAKE_CURRENT_LIST_DIR}/bin/${BUILD_PLATFORM}/${CMAKE_BUILD_TYPE}/lib
)

# Set headers for Hako
set(HEADERS
    inc/Hako.h
    inc/HakoCmd.h
    inc/HakoFile.h
    inc/HakoPlatforms.h
    inc/IFile.h
    inc/Serializer.h
    private/HakoLog.h
    private/SerializerList.h
    private/MurmurHash3.h
)

# Set sources for Hako
set(SOURCES
    src/Hako.cpp
    src/HakoCmd.cpp
    src/HakoFile.cpp
    src/IFile.cpp
    src/Serializer.cpp
    private/HakoLog.cpp
    private/SerializerList.cpp
    private/MurmurHash3.cpp
)

if(HAKO_READ_OUTSIDE_OF_ARCHIVE)
    add_compile_definitions("HAKO_READ_OUTSIDE_OF_ARCHIVE")
endif(HAKO_READ_OUTSIDE_OF_ARCHIVE)

if(HAKO_NO_DYNAMIC_SERIALIZERS)
    add_compile_definitions("HAKO_NO_DYNAMIC_SERIALIZERS")
endif(HAKO_NO_DYNAMIC_SERIALIZERS)

if(HAKO_NO_LOG)
    add_compile_definitions("HAKO_NO_LOG")
endif(HAKO_NO_LOG)

# Create library for Hako
add_library(Hako ${SOURCES} ${HEADERS})

# Add include directories
target_include_directories(Hako PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)
target_include_directories(Hako PRIVATE ${CMAKE_CURRENT_LIST_DIR}/private)

if(HAKO_STANDALONE)
	# Add source to this project's executable.
	add_executable(HakoExe "main.cpp")

	target_link_libraries(HakoExe PRIVATE Hako)
    set_target_properties(HakoExe PROPERTIES RUNTIME_OUTPUT_NAME Hako)
endif(HAKO_STANDALONE)
