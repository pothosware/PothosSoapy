///
/// \file PothosSDR/Registry.hpp
///
/// Device factory registration API.
///
/// \copyright
/// Copyright (c) 2014-2014 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <PothosSDR/Config.hpp>
#include <string>
#include <vector>
#ifdef POTHOS_SDR_BOOST
#include <boost/function.hpp>
#else
#include <functional> //function
#endif

namespace PothosSDR
{
    //! forward declaration of sdr device
    class SDRDevice;

    /*!
     * A registry object loads device functions into the global registry.
     */
    class POTHOS_SDR_API Registry
    {
    public:

        #ifdef POTHOS_SDR_BOOST
        typedef boost::function<std::vector<std::string>(void)> FindFunction;
        typedef boost::function<boost::shared_ptr<SDRDevice>(const std::string &)> MakeFunction;
        #else
        typedef std::function<std::vector<std::string>(void)> FindFunction;
        typedef std::function<std::shared_ptr<SDRDevice>(const std::string &)> MakeFunction;
        #endif

        /*!
         * Register an SDR device find and make function.
         * \param name a unique name to identify the device
         * \param find the find function returns arg list
         * \param make the make function returns device sptr
         */
        Registry(const std::string &name, const FindFunction &find, const MakeFunction &make);
    };
}
