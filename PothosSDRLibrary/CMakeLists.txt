########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8)
project(PothosSDRLibrary)
enable_language(CXX)
enable_testing()

########################################################################
# Install headers
########################################################################
install(
    DIRECTORY include/PothosSDR
    DESTINATION include
)

########################################################################
# Build the library
########################################################################
set(POTHOS_SDR_SOURCES
    lib/Interface.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(PothosSDR SHARED ${POTHOS_SDR_SOURCES})
#target_link_libraries(PothosSDR ...)
#set_target_properties(PothosSDR PROPERTIES VERSION ${POTHOS_SDR_VERSION})
set_target_properties(PothosSDR PROPERTIES DEFINE_SYMBOL "POTHOS_SDR_DLL_EXPORTS")

########################################################################
# Install the library
########################################################################
install(TARGETS PothosSDR
    LIBRARY DESTINATION lib${LIB_SUFFIX} # .so file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
    RUNTIME DESTINATION bin              # .dll file
)
