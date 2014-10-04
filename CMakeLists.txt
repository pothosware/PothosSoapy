########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(PothosSDR)
enable_language(CXX)

if (NOT POTHOS_MODULE_PATH)
    set(POTHOS_MODULE_PATH "${CMAKE_INSTALL_PREFIX}/share/Pothos/cmake/Modules")
endif()

include(${POTHOS_MODULE_PATH}/PothosUtil.cmake)

########################################################################
## Find SoapySDR
########################################################################

#TODO

########################################################################
## Build and install module
########################################################################
POTHOS_MODULE_UTIL(
    TARGET SDRSupport
    SOURCES SDRSource.cpp SDRSink.cpp
    LIBRARIES ${SOAPY_SDR_LIBRARIES}
    DESTINATION blocks/sdr
    ENABLE_DOCS
)
