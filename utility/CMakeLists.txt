########################################################################
## Feature registration
########################################################################
cmake_dependent_option(ENABLE_SOAPY_UTILITY "Enable Pothos Soapy.Utility component" ON "ENABLE_SOAPY" OFF)
add_feature_info("  Utility" ENABLE_SOAPY_UTILITY "Utility blocks that operate on SDR streams")
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
        RandomDropper.cpp
        TxBurstTimer.cpp
    DESTINATION soapy
    ENABLE_DOCS
)
