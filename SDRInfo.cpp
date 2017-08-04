// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Device.hpp>
#include <json.hpp>

using json = nlohmann::json;

static std::string enumerateSDRDevices(void)
{
    json topObject;
    auto &infoObject = topObject["SoapySDR info"];

    //install info
    infoObject["API Version"] = SoapySDR::getAPIVersion();
    infoObject["ABI Version"] = SoapySDR::getABIVersion();
    infoObject["Install Root"] = SoapySDR::getRootPath();

    //list of device factories
    SoapySDR::loadModules();
    std::string factories;
    for (const auto &factory : SoapySDR::Registry::listFindFunctions())
    {
        if (not factories.empty()) factories += ", ";
        factories += factory.first;
    }
    infoObject["Factories"] = factories;

    //available devices
    json devicesArray(json::array());
    for (const auto &result : SoapySDR::Device::enumerate())
    {
        json deviceObject(json::object());
        for (const auto &kwarg : result)
        {
            deviceObject[kwarg.first] = kwarg.second;
        }
        devicesArray.push_back(deviceObject);
    }
    if (not devicesArray.empty()) topObject["SDR Device"] = devicesArray;

    return topObject.dump();
}

pothos_static_block(registerSDRInfo)
{
    Pothos::PluginRegistry::addCall(
        "/devices/sdr/info", &enumerateSDRDevices);
}
