########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(PothosSoapy CXX)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    find_package(Pothos "0.7.0" CONFIG REQUIRED)
else()
    find_package(Pothos CONFIG REQUIRED) #in-tree build
endif()

########################################################################
# json.hpp header
########################################################################
find_path(JSON_HPP_INCLUDE_DIR NAMES json.hpp PATH_SUFFIXES nlohmann)

if (NOT JSON_HPP_INCLUDE_DIR)
    message(WARNING "Pothos SDR toolkit requires json.hpp, skipping...")
endif (NOT JSON_HPP_INCLUDE_DIR)

########################################################################
## Find SoapySDR
########################################################################
find_package(SoapySDR "0.6.0" CONFIG)

if (SoapySDR_FOUND)
    message(STATUS "SoapySDR_VERSION: ${SoapySDR_VERSION}")
    message(STATUS "SoapySDR_INCLUDE_DIRS: ${SoapySDR_INCLUDE_DIRS}")
    message(STATUS "SoapySDR_LIBRARIES: ${SoapySDR_LIBRARIES}")
else()
    message(WARNING "Pothos SDR blocks require SoapySDR, skipping...")
endif()

########################################################################
## Feature registration
########################################################################
include(FeatureSummary)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_SOAPY "Enable Pothos SDR component" ON "Pothos_FOUND;SoapySDR_FOUND;JSON_HPP_INCLUDE_DIR" OFF)
add_feature_info(Soapy ENABLE_SOAPY "SDR source and sink blocks for radio hardware")
if (NOT ENABLE_SOAPY)
    return()
endif()

########################################################################
## Generate block descriptions
########################################################################
set(TITLE "Source")
set(CATEGORY "/Sources")
set(ALIAS "/sdr/source")
set(PATH "/soapy/source")
set(XCVR "receiver")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapyBlockDesc.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/SoapySourceDesc.hpp
@ONLY)

set(TITLE "Sink")
set(CATEGORY "/Sinks")
set(ALIAS "/sdr/sink")
set(PATH "/soapy/sink")
set(XCVR "transmitter")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapyBlockDesc.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/SoapySinkDesc.hpp
@ONLY)

########################################################################
## Build and install module
########################################################################
include_directories(${SoapySDR_INCLUDE_DIRS})
include_directories(${JSON_HPP_INCLUDE_DIR})
POTHOS_MODULE_UTIL(
    TARGET SoapySupport
    SOURCES
        SoapyBlock.cpp
        SoapySource.cpp
        SoapySink.cpp
        DemoController.cpp
        Logger.cpp
        SoapyInfo.cpp
        BlockThread.cpp
        EnumerateCache.cpp
    LIBRARIES ${SoapySDR_LIBRARIES}
    DESTINATION soapy
    DOC_SOURCES
        DemoController.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/SoapySourceDesc.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/SoapySinkDesc.hpp
)

########################################################################
## Build subdirectories
########################################################################
add_subdirectory(utility)
