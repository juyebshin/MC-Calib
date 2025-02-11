cmake_minimum_required(VERSION 2.8.3)
project(create_charuco)

add_definitions(-std=c++14)
set(CMAKE_CXX_FLAGS "-std=c++14") # required for Ceres https://github.com/colmap/colmap/issues/905#issuecomment-731138700
set(CMAKE_BUILD_TYPE Release)
set(DCMAKE_BUILD_TYPE Release)
set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE} CACHE STRING "" FORCE)

find_package(OpenCV REQUIRED)
include_directories( ${OpenCV_INCLUDE_DIRS} )

find_package(Ceres REQUIRED)

# boost related setup
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED log)
message(STATUS "Boost version: ${Boost_VERSION}")

# This is needed if your Boost version is newer than your CMake version
# or if you have an old version of CMake (<3.5)
if(NOT TARGET Boost::filesystem)
    add_library(Boost::filesystem IMPORTED INTERFACE)
    set_property(TARGET Boost::filesystem PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::filesystem PROPERTY
        INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})
endif()


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

include_directories(
	include
	/usr/include/opencv
	/usr/include/eigen3
	/usr/local/lib
)

######################### Documentation related ##########################
find_package(Doxygen)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
  message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)


##########################################################################

## Single calibration camera + cube
add_executable(calibrate src/main_calibrate.cpp
				src/point_refinement.h
				src/geometrytools.hpp
				src/geometrytools.cpp
				src/Calibration.hpp
				src/Calibration.cpp
				src/Camera.hpp
				src/Camera.cpp
				src/Board.hpp
				src/Board.cpp
				src/CameraObs.hpp
				src/CameraObs.cpp
				src/Frame.hpp
				src/Frame.cpp
				src/BoardObs.hpp
				src/BoardObs.cpp
				src/Object3D.hpp
				src/Object3D.cpp
				src/Object3DObs.cpp
				src/Object3DObs.hpp
				src/CameraGroup.cpp
				src/CameraGroup.hpp
				src/CameraGroupObs.cpp
				src/CameraGroupObs.hpp
				src/OptimizationCeres.h
				src/logger.h
				src/logger.cpp
				src/Graph.hpp
				src/Graph.cpp)

target_link_libraries(calibrate
 -L/usr/local/lib ${OpenCV_LIBS} ${CERES_LIBRARIES} Boost::log
 )

##################Generate Charuco######################
add_executable(generate_charuco src/main_create_charuco.cpp)

target_link_libraries(generate_charuco
 -L/usr/local/lib ${OpenCV_LIBS} ${CERES_LIBRARIES} Boost::log
 )

# unit tests related
add_subdirectory(tests)
