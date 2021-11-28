# Minimum CMake version
cmake_minimum_required (VERSION 3.8)

# Create the project
project ("Hako" LANGUAGES CXX)

# Set the C++ language standard
set(CMAKE_CXX_STANDARD
	14
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

# Set compiler options
if(MSVC)
    add_compile_options("/W4")
endif(MSVC)

# Include sub-folders
add_subdirectory("Hako")

if(HAKO_STANDALONE)
	# Add source to this project's executable.
	add_executable (HakoContentPacker "main.cpp")

	target_link_libraries(HakoContentPacker PRIVATE Hako)
endif(HAKO_STANDALOE)
