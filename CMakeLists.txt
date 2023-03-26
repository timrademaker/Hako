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

# Include sub-folders
add_subdirectory("Hako")

if(HAKO_STANDALONE)
	# Add source to this project's executable.
	add_executable (Hako "main.cpp")

	target_link_libraries(Hako PRIVATE HakoLib)
endif(HAKO_STANDALONE)
