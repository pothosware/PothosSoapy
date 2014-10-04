///
/// \file PothosSDR/Config.hpp
///
/// Common macro definitions for Pothos SDR library API export.
///
/// \copyright
/// Copyright (c) 2014-2014 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
// http://gcc.gnu.org/wiki/Visibility
// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
  #define POTHOS_SDR_HELPER_DLL_IMPORT __declspec(dllimport)
  #define POTHOS_SDR_HELPER_DLL_EXPORT __declspec(dllexport)
  #define POTHOS_SDR_HELPER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define POTHOS_SDR_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define POTHOS_SDR_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define POTHOS_SDR_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define POTHOS_SDR_HELPER_DLL_IMPORT
    #define POTHOS_SDR_HELPER_DLL_EXPORT
    #define POTHOS_SDR_HELPER_DLL_LOCAL
  #endif
#endif

// Now we use the generic helper definitions above to define POTHOS_SDR_API and POTHOS_SDR_LOCAL.
// POTHOS_SDR_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for static build)
// POTHOS_SDR_LOCAL is used for non-api symbols.

#define POTHOS_SDR_DLL //always building a DLL

#ifdef POTHOS_SDR_DLL // defined if POTHOS is compiled as a DLL
  #ifdef POTHOS_SDR_DLL_EXPORTS // defined if we are building the POTHOS DLL (instead of using it)
    #define POTHOS_SDR_API POTHOS_SDR_HELPER_DLL_EXPORT
    #define POTHOS_SDR_EXTERN
  #else
    #define POTHOS_SDR_API POTHOS_SDR_HELPER_DLL_IMPORT
    #define POTHOS_SDR_EXTERN extern
  #endif // POTHOS_SDR_DLL_EXPORTS
  #define POTHOS_SDR_LOCAL POTHOS_SDR_HELPER_DLL_LOCAL
#else // POTHOS_SDR_DLL is not defined: this means POTHOS is a static lib.
  #define POTHOS_SDR_API
  #define POTHOS_SDR_LOCAL
  #define POTHOS_SDR_EXTERN
#endif // POTHOS_SDR_DLL
