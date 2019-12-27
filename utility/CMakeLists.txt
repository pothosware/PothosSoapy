########################################################################
## Feature registration
########################################################################
cmake_dependent_option(ENABLE_SOAPY_UTILITY "Enable Pothos Soapy.Utility component" ON "ENABLE_SOAPY" OFF)
add_feature_info("  SDR Utils" ENABLE_SOAPY_UTILITY "Utility blocks that operate on SDR streams")
if (NOT ENABLE_SOAPY_UTILITY)
    return()
endif()

########################################################################
## Utility blocks
########################################################################
POTHOS_MODULE_UTIL(
    TARGET SoapyUtilityBlocks
    SOURCES
        ChannelAligner.cpp
        Converter.cpp
        RandomDropper.cpp
        TxBurstTimer.cpp
    LIBRARIES ${SoapySDR_LIBRARIES}
    DESTINATION soapy
    ENABLE_DOCS
)
