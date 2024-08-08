cmake_minimum_required(VERSION 3.16)

project(TcxViewer CXX)

set(CXX ON)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET(PROJECT_CMAKE_SEARCH_PATH "C:/Qt/6.7.2/msvc2019_64" CACHE PATH "Additional Qt search path" )

list(APPEND CMAKE_PREFIX_PATH "${PROJECT_CMAKE_SEARCH_PATH}/lib/cmake/")
list(APPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}")

# Visual Studio compiler with static runtime libraries
if(MSVC AND MT)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
endif()

if (MSVC)
	add_definitions(/std:c++20)
	add_definitions(/DNOMINMAX)
	add_definitions(/MP)
	add_definitions(/DPSAPI_VERSION=1)
	add_definitions(/D_CRT_SECURE_NO_WARNINGS)
	add_definitions(/W4)
else()
	add_definitions(-std=c++20)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Charts Core Graphs Gui Xml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Charts Core Graphs Gui Xml)

message(STATUS "Using Qt version ${QT_VERSION_MAJOR}.")

if ("${QT_VERSION_MAJOR}" STREQUAL "6")
	qt_standard_project_setup()
endif()

# Add base folder for better inclusion paths
include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")

# Main Sources
file(GLOB PROJECT_HEADERS ${PROJECT_SOURCE_DIR}/src/*.hpp)
file(GLOB PROJECT_SOURCES_CPP ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB PROJECT_SOURCES_UI ${PROJECT_SOURCE_DIR}/src/*.ui)


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
	qt_add_executable(${CMAKE_PROJECT_NAME}
		MANUAL_FINALIZATION
		${PROJECT_HEADERS} ${PROJECT_SOURCES_CPP} ${PROJECT_SOURCES_UI}
	)
else()
	add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_HEADERS} ${PROJECT_SOURCES_CPP})
endif()

set(CMAKE_CXX_STANDARD 20)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Charts Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Graphs Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Xml)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(${CMAKE_PROJECT_NAME})
endif()
