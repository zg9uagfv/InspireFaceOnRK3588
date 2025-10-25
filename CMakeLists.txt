cmake_minimum_required(VERSION 3.10)
project(InspireFaceDemo)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find required packages
find_package(OpenCV REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add the executables
add_executable(camera_face_recognizer camera_face_recognizer.cpp)
add_executable(add_face_to_database add_face_to_database.cpp)
add_executable(check_database check_database.cpp)

# Link libraries
target_link_libraries(camera_face_recognizer 
    ${OpenCV_LIBS}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libInspireFace.so
)

target_link_libraries(add_face_to_database 
    ${OpenCV_LIBS}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libInspireFace.so
)

target_link_libraries(check_database 
    ${OpenCV_LIBS}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libInspireFace.so
)

# Set RPATH so the executable can find the shared library at runtime
set_target_properties(camera_face_recognizer PROPERTIES
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

set_target_properties(add_face_to_database PROPERTIES
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

set_target_properties(check_database PROPERTIES
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Compiler options
target_compile_options(camera_face_recognizer PRIVATE 
    -Wall 
    -Wextra
)

target_compile_options(add_face_to_database PRIVATE 
    -Wall 
    -Wextra
)

# Debug configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(camera_face_recognizer PRIVATE -g -O0)
    target_compile_options(add_face_to_database PRIVATE -g -O0)
else()
    target_compile_options(camera_face_recognizer PRIVATE -O3)
    target_compile_options(add_face_to_database PRIVATE -O3)
endif()