// Copyright (c) 2014-2019 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyBlock.hpp"
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Errors.hpp>
#include <Poco/Format.h>
#include <cassert>
#include <chrono>
#include <json.hpp>

using json = nlohmann::json;

#ifdef _MSC_VER
#  define current_func() std::string(__FUNCTION__)
#else
#  define current_func() std::string(__PRETTY_FUNCTION__)
#endif

#define check_device_ptr() {if (_device == nullptr) \
    throw Pothos::NullPointerException(Poco::format("%s - device not setup!", current_func()));}

#define check_stream_ptr() {if (_stream == nullptr) \
    throw Pothos::NullPointerException(Poco::format("%s - stream not setup!", current_func()));}

SoapyBlock::SoapyBlock(const int direction, const Pothos::DType &dtype, const std::vector<size_t> &chs):
    _logger(Poco::Logger::get("SoapyBlock")),
    _backgrounding(false),
    _activateWaits(false),
    _eventSquash(false),
    _autoActivate(true),
    _direction(direction),
    _dtype(dtype),
    _channels(chs.empty()?std::vector<size_t>(1, 0):chs),
    _device(nullptr),
    _stream(nullptr),
    _enableStatus(false),
    _pendingLabels(_channels.size())
{
    assert(not _channels.empty());
    if (SoapySDR::getABIVersion() != SOAPY_SDR_ABI_VERSION) throw Pothos::Exception("SoapyBlock::make()",
        Poco::format("Failed ABI check. Pothos SDR %s. Soapy SDR %s. Rebuild the module.",
        std::string(SOAPY_SDR_ABI_VERSION), SoapySDR::getABIVersion()));

    //hooks for overlay
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, overlay));

    //threading options
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setCallingMode));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setEventSquash));

    //streaming
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setupDevice));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setupStream));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setSampleRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getSampleRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getSampleRates));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setFrontendMap));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getFrontendMap));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setAutoActivate));
    this->registerCallable("streamControl", Pothos::Callable(&SoapyBlock::streamControl).bind(std::ref(*this), 0)); //3 arg version
    this->registerCallable("streamControl", Pothos::Callable(&SoapyBlock::streamControl).bind(std::ref(*this), 0).bind(0, 3)); //2 arg version
    this->registerCallable("streamControl", Pothos::Callable(&SoapyBlock::streamControl).bind(std::ref(*this), 0).bind(0, 2).bind(0, 3)); //1 arg version
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setEnableStatus));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setGlobalSettings));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setGlobalSetting));

    //channels -- called by setters
    this->registerCall(this, "setFrequency",       &SoapyBlock::setFrequency);
    this->registerCall(this, "setFrequency",       &SoapyBlock::setFrequencyArgs);
    this->registerCall(this, "setFrequency",       &SoapyBlock::setFrequencies);
    this->registerCall(this, "setFrequency",       &SoapyBlock::setFrequenciesArgs);
    this->registerCall(this, "setGainMode",        &SoapyBlock::setGainMode);
    this->registerCall(this, "setGainMode",        &SoapyBlock::setGainModes);
    this->registerCall(this, "setGain",            &SoapyBlock::setGain);
    this->registerCall(this, "setGain",            &SoapyBlock::setGainMap);
    this->registerCall(this, "setGain",            &SoapyBlock::setGains);
    this->registerCall(this, "setAntenna",         &SoapyBlock::setAntenna);
    this->registerCall(this, "setAntenna",         &SoapyBlock::setAntennas);
    this->registerCall(this, "setBandwidth",       &SoapyBlock::setBandwidth);
    this->registerCall(this, "setBandwidth",       &SoapyBlock::setBandwidths);
    this->registerCall(this, "setDCOffsetMode",    &SoapyBlock::setDCOffsetMode);
    this->registerCall(this, "setDCOffsetMode",    &SoapyBlock::setDCOffsetModes);
    this->registerCall(this, "setDCOffsetAdjust",  &SoapyBlock::setDCOffsetAdjust);
    this->registerCall(this, "setChannelSettings", &SoapyBlock::setChannelSettings);
    this->registerCall(this, "setChannelSettings", &SoapyBlock::setChannelSettingsArgs);
    this->registerCall(this, "setChannelSetting",  &SoapyBlock::setChannelSetting);

    //channels
    for (size_t i = 0; i < _channels.size(); i++)
    {
        const auto chanStr = std::to_string(i);
        //freq overall with tune args
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SoapyBlock::setFrequencyChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SoapyBlock::setFrequencyChanArgs).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getFrequency"+chanStr, Pothos::Callable(&SoapyBlock::getFrequency).bind(std::ref(*this), 0).bind(i, 1));
        //freq component by name
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SoapyBlock::setFrequencyName).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SoapyBlock::setFrequencyNameArgs).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getFrequency"+chanStr, Pothos::Callable(&SoapyBlock::getFrequencyChan).bind(std::ref(*this), 0).bind(i, 1));
        //gain by name
        this->registerCallable("setGain"+chanStr, Pothos::Callable(&SoapyBlock::setGainName).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGain"+chanStr, Pothos::Callable(&SoapyBlock::getGainName).bind(std::ref(*this), 0).bind(i, 1));
        //gain overall
        this->registerCallable("setGain"+chanStr, Pothos::Callable(&SoapyBlock::setGainChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGain"+chanStr, Pothos::Callable(&SoapyBlock::getGain).bind(std::ref(*this), 0).bind(i, 1));
        //gain dict
        this->registerCallable("setGain"+chanStr, Pothos::Callable(&SoapyBlock::setGainChanMap).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGainNames"+chanStr, Pothos::Callable(&SoapyBlock::getGainNames).bind(std::ref(*this), 0).bind(i, 1));
        //gain mode
        this->registerCallable("setGainMode"+chanStr, Pothos::Callable(&SoapyBlock::setGainModeChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGainMode"+chanStr, Pothos::Callable(&SoapyBlock::getGainMode).bind(std::ref(*this), 0).bind(i, 1));
        //antenna
        this->registerCallable("setAntenna"+chanStr, Pothos::Callable(&SoapyBlock::setAntennaChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getAntenna"+chanStr, Pothos::Callable(&SoapyBlock::getAntenna).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getAntennas"+chanStr, Pothos::Callable(&SoapyBlock::getAntennas).bind(std::ref(*this), 0).bind(i, 1));
        //bandwidth
        this->registerCallable("setBandwidth"+chanStr, Pothos::Callable(&SoapyBlock::setBandwidthChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getBandwidth"+chanStr, Pothos::Callable(&SoapyBlock::getBandwidth).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getBandwidths"+chanStr, Pothos::Callable(&SoapyBlock::getBandwidths).bind(std::ref(*this), 0).bind(i, 1));
        //dc offset mode
        this->registerCallable("setDCOffsetMode"+chanStr, Pothos::Callable(&SoapyBlock::setDCOffsetModeChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getDCOffsetMode"+chanStr, Pothos::Callable(&SoapyBlock::getDCOffsetMode).bind(std::ref(*this), 0).bind(i, 1));
        //dc offset adjust
        this->registerCallable("setDCOffsetAdjust"+chanStr, Pothos::Callable(&SoapyBlock::setDCOffsetAdjustChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getDCOffsetAdjust"+chanStr, Pothos::Callable(&SoapyBlock::getDCOffsetAdjust).bind(std::ref(*this), 0).bind(i, 1));
        //sensors
        this->registerCallable("getSensors"+chanStr, Pothos::Callable(&SoapyBlock::getSensorsChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getSensor"+chanStr, Pothos::Callable(&SoapyBlock::getSensorChan).bind(std::ref(*this), 0).bind(i, 1));
        //settings
        this->registerCallable("setChannelSetting"+chanStr, Pothos::Callable(&SoapyBlock::setChannelSettingChan).bind(std::ref(*this), 0).bind(i, 1));

        //channel probes
        this->registerProbe("getFrequency"+chanStr);
        this->registerProbe("getGain"+chanStr);
        this->registerProbe("getGainNames"+chanStr);
        this->registerProbe("getGainMode"+chanStr);
        this->registerProbe("getAntenna"+chanStr);
        this->registerProbe("getAntennas"+chanStr);
        this->registerProbe("getBandwidth"+chanStr);
        this->registerProbe("getBandwidths"+chanStr);
        this->registerProbe("getDCOffsetMode"+chanStr);
        this->registerProbe("getDCOffsetAdjust"+chanStr);
        this->registerProbe("getSensors"+chanStr);
        this->registerProbe("getSensor"+chanStr);
    }

    //clocking
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setClockRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getClockRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setClockSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getClockSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getClockSources));

    //time
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setTimeSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getTimeSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getTimeSources));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setHardwareTime));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getHardwareTime));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setCommandTime));
    this->registerCallable("setHardwareTime", Pothos::Callable(&SoapyBlock::setHardwareTime).bind(std::ref(*this), 0).bind(std::string(), 2));
    this->registerCallable("getHardwareTime", Pothos::Callable(&SoapyBlock::getHardwareTime).bind(std::ref(*this), 0).bind(std::string(), 1));

    //sensors
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getSensors));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getSensor));

    //gpio
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getGpioBanks));
    this->registerCallable("setGpioConfig", Pothos::Callable(&SoapyBlock::setGpioConfigs).bind(std::ref(*this), 0));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, setGpioConfig));
    this->registerCall(this, POTHOS_FCN_TUPLE(SoapyBlock, getGpioValue));

    //probes
    this->registerProbe("getSampleRate");
    this->registerProbe("getSampleRates");
    this->registerProbe("getFrontendMap");
    this->registerProbe("getClockRate");
    this->registerProbe("getClockSource");
    this->registerProbe("getClockSources");
    this->registerProbe("getTimeSource");
    this->registerProbe("getTimeSources");
    this->registerProbe("getHardwareTime");
    this->registerProbe("getSensor");
    this->registerProbe("getSensors");
    this->registerProbe("getGpioBanks");
    this->registerProbe("getGpioValue");

    //status
    this->registerSignal("status");

    //start eval thread
    _evalThreadDone = false;
    _evalErrorValid = false;
    _evalThread = std::thread(&SoapyBlock::evalThreadLoop, this);
}

static std::mutex &getMutex(void)
{
    static std::mutex mutex;
    return mutex;
}

/*!
 * Get a list of enumerated devices.
 * Use caching and an expired timeout to avoid over querying.
 * This helps when the gui checks the overlay for many blocks.
 * \return a copy of the cached result for thread safety
 */
SoapySDR::KwargsList cachedEnumerate(void);

static json optionsToComboBox(
    const std::string &paramKey,
    const std::vector<std::string> &options)
{
    json paramObj;
    paramObj["key"] = paramKey;
    auto &optionsArray = paramObj["options"];
    paramObj["widgetKwargs"]["editable"] = true;
    paramObj["widgetType"] = "ComboBox";

    //a default option for empty/unspecified
    json defaultOption;
    defaultOption["name"] = "Default";
    defaultOption["value"] = "\"\"";
    optionsArray.push_back(defaultOption);

    //add each available option
    for (const auto &name : options)
    {
        json option;
        option["name"] = name;
        option["value"] = "\"" + name + "\"";
        optionsArray.push_back(option);
    }

    return paramObj;
}

std::string SoapyBlock::overlay(void) const
{
    json topObj;

    auto &params = topObj["params"];

    //editable drop down for user-controlled input
    json deviceArgsParam;
    deviceArgsParam["key"] = "deviceArgs";
    auto &deviceArgsOpts = deviceArgsParam["options"];
    deviceArgsParam["widgetKwargs"]["editable"] = true;
    deviceArgsParam["widgetType"] = "ComboBox";

    //a default option for empty/unspecified device
    json defaultOption;
    defaultOption["name"] = "Null Device";
    defaultOption["value"] = "{\"driver\":\"null\"}";
    deviceArgsOpts.push_back(defaultOption);

    //enumerate devices and add to the options list
    for (const auto &args : cachedEnumerate())
    {
        //create args dictionary
        std::string value;
        for (const auto &pair : args)
        {
            if (not value.empty()) value += ", ";
            value += "\"" + pair.first + "\" : \"" + pair.second + "\"";
        }

        //create displayable name
        //use the standard label convention, but fall-back on driver/serial
        std::string name;
        if (args.count("label") != 0) name = args.at("label");
        else if (args.count("driver") != 0)
        {
            name = args.at("driver");
            if (args.count("serial") != 0) name += " " + args.at("serial");
        }
        else continue; //shouldn't happen

        json option;
        option["name"] = name;
        option["value"] = "{"+value+"}";
        deviceArgsOpts.push_back(option);
    }
    params.push_back(deviceArgsParam);

    //drop-down options
    params.push_back(optionsToComboBox("antenna", _antennaOptions));
    params.push_back(optionsToComboBox("clockSource", _clockOptions));
    params.push_back(optionsToComboBox("timeSource", _timeOptions));

    return topObj.dump();
}

void SoapyBlock::setupDevice(const Pothos::ObjectKwargs &deviceArgs)
{
    //protect device make -- its not thread safe
    std::unique_lock<std::mutex> lock(getMutex());
    _device = SoapySDR::Device::make(_toKwargs(deviceArgs));
    _antennaOptions = _device->listAntennas(_direction, _channels.front());
    _timeOptions = _device->listTimeSources();
    _clockOptions = _device->listClockSources();
}

SoapyBlock::~SoapyBlock(void)
{
    //stop the status thread if enabled
    this->setEnableStatus(false);

    //close the stream, the stream should be stopped by deactivate
    //but this actually cleans up and frees the stream object
    if (_stream != nullptr) _device->closeStream(_stream);

    //stop the eval thread before cleaning up
    _evalThreadDone = true;
    _cond.notify_one();
    _evalThread.join();

    //now with the mutex locked, the device object can be released
    std::unique_lock<std::mutex> lock(getMutex());
    if (_device != nullptr) SoapySDR::Device::unmake(_device);
}

/*******************************************************************
 * Stream config
 ******************************************************************/
void SoapyBlock::setupStream(const Pothos::ObjectKwargs &streamArgs)
{
    //create format string from the dtype
    std::string format;
    if (_dtype.isComplex()) format += "C";
    if (_dtype.isFloat()) format += "F";
    else if (_dtype.isInteger() and _dtype.isSigned()) format += "S";
    else if (_dtype.isInteger() and not _dtype.isSigned()) format += "U";
    size_t bits = _dtype.elemSize()*8;
    if (_dtype.isComplex()) bits /= 2;
    format += std::to_string(bits);

    //create the stream
    _stream = _device->setupStream(_direction, format, _channels, _toKwargs(streamArgs));
}

void SoapyBlock::setSampleRate(const double rate)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++)
    {
        _device->setSampleRate(_direction, _channels.at(i), rate);
        _pendingLabels[i]["rxRate"] = Pothos::Object(_device->getSampleRate(_direction, _channels.at(i)));
    }
}

double SoapyBlock::getSampleRate(void) const
{
    check_device_ptr();
    return _device->getSampleRate(_direction, _channels.front());
}

std::vector<double> SoapyBlock::getSampleRates(void) const
{
    check_device_ptr();
    return _device->listSampleRates(_direction, _channels.front());
}

void SoapyBlock::setAutoActivate(const bool autoActivate)
{
    _autoActivate = autoActivate;
}

void SoapyBlock::streamControl(const std::string &what, const long long timeNs, const size_t numElems)
{
    check_stream_ptr();
    int r = 0;
    if (what == "ACTIVATE")          r = _device->activateStream(_stream, 0, timeNs, numElems);
    if (what == "ACTIVATE_AT")       r = _device->activateStream(_stream, SOAPY_SDR_HAS_TIME, timeNs, numElems);
    if (what == "ACTIVATE_BURST")    r = _device->activateStream(_stream, SOAPY_SDR_END_BURST, timeNs, numElems);
    if (what == "ACTIVATE_BURST_AT") r = _device->activateStream(_stream, SOAPY_SDR_HAS_TIME | SOAPY_SDR_END_BURST, timeNs, numElems);
    if (what == "DEACTIVATE")        r = _device->deactivateStream(_stream, 0, timeNs);
    if (what == "DEACTIVATE_AT")     r = _device->deactivateStream(_stream, SOAPY_SDR_HAS_TIME, timeNs);
    if (r != 0) throw Pothos::Exception("SoapyBlock::streamControl("+what+")", "de/activateStream returned " + std::to_string(r));
}

void SoapyBlock::setEnableStatus(const bool enable)
{
    _enableStatus = enable;
    this->configureStatusThread();
}

void SoapyBlock::forwardStatusLoop(void)
{
    int ret = 0;
    size_t chanMask = 0;
    int flags = 0;
    long long timeNs = 0;

    while (this->isActive() and _enableStatus)
    {
        ret = _device->readStreamStatus(_stream, chanMask, flags, timeNs);
        if (ret == SOAPY_SDR_TIMEOUT) continue;

        Pothos::ObjectKwargs status;
        status["ret"] = Pothos::Object(ret);
        if (chanMask != 0) status["chanMask"] = Pothos::Object(chanMask);
        status["flags"] = Pothos::Object(flags);
        if ((flags & SOAPY_SDR_HAS_TIME) != 0) status["timeNs"] = Pothos::Object(timeNs);
        if ((flags & SOAPY_SDR_END_BURST) != 0) status["endBurst"];
        if (ret != 0) status["error"] = Pothos::Object(SoapySDR::errToStr(ret));

        //emit the status signal
        this->emitSignal("status", status);

        //exit the thread if stream status is not supported
        //but only after reporting this to "status" signal
        if (ret == SOAPY_SDR_NOT_SUPPORTED) return;
    }
}

void SoapyBlock::configureStatusThread(void)
{
    //ensure thread is running
    if (this->isActive() and _enableStatus)
    {
        if (_statusMonitor.joinable()) return;
        _statusMonitor = std::thread(&SoapyBlock::forwardStatusLoop, this);
    }

    //ensure thread is stopped
    else
    {
        if (not _statusMonitor.joinable()) return;
        _statusMonitor.join();
    }
}

/*******************************************************************
 * Frontend map
 ******************************************************************/
void SoapyBlock::setFrontendMap(const std::string &mapping)
{
    check_device_ptr();
    if (mapping.empty()) return;
    _device->setFrontendMapping(_direction, mapping);
    _antennaOptions = _device->listAntennas(_direction, _channels.front());
}

std::string SoapyBlock::getFrontendMap(void) const
{
    check_device_ptr();
    return _device->getFrontendMapping(_direction);
}

/*******************************************************************
 * Frequency
 ******************************************************************/

//-------- setFrequency(no tune args) ----------//

void SoapyBlock::setFrequency(const double freq)
{
    check_device_ptr();
    this->setFrequencyArgs(freq, _cachedTuneArgs[0]);
}

void SoapyBlock::setFrequencies(const std::vector<double> &freqs)
{
    check_device_ptr();
    this->setFrequenciesArgs(freqs, _cachedTuneArgs[0]);
}

void SoapyBlock::setFrequencyChan(const size_t chan, const double freq)
{
    check_device_ptr();
    this->setFrequencyChanArgs(chan, freq, _cachedTuneArgs[chan]);
}

void SoapyBlock::setFrequencyName(const size_t chan, const std::string &name, const double freq)
{
    check_device_ptr();
    this->setFrequencyNameArgs(chan, name, freq, _cachedTuneArgs[chan]);
}

//-------- setFrequency(tune args) ----------//

void SoapyBlock::setFrequencyArgs(const double freq, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setFrequencyChanArgs(i, freq, args);
}

void SoapyBlock::setFrequenciesArgs(const std::vector<double> &freqs, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    for (size_t i = 0; i < freqs.size(); i++) this->setFrequencyChanArgs(i, freqs[i], args);
}

void SoapyBlock::setFrequencyChanArgs(const size_t chan, const double freq, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    _cachedTuneArgs[chan] = args;
    _device->setFrequency(_direction, _channels.at(chan), freq, _toKwargs(args));
    _pendingLabels[chan]["rxFreq"] = Pothos::Object(_device->getFrequency(_direction, _channels.at(chan)));
}

void SoapyBlock::setFrequencyNameArgs(const size_t chan, const std::string &name, const double freq, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    _cachedTuneArgs[chan] = args;
    _device->setFrequency(_direction, _channels.at(chan), name, freq, _toKwargs(args));
}

//-------- getFrequency ----------//

double SoapyBlock::getFrequency(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getFrequency(_direction, _channels.at(chan));
}

double SoapyBlock::getFrequencyChan(const size_t chan, const std::string &name) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getFrequency(_direction, _channels.at(chan), name);
}

/*******************************************************************
 * Gain mode
 ******************************************************************/
void SoapyBlock::setGainMode(const bool automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setGainModeChan(i, automatic);
}

void SoapyBlock::setGainModes(const std::vector<bool> &automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < automatic.size(); i++) this->setGainModeChan(i, automatic[i]);
}

void SoapyBlock::setGainModeChan(const size_t chan, const bool automatic)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setGainMode(_direction, _channels.at(chan), automatic);
}

double SoapyBlock::getGainMode(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return false;
    return _device->getGainMode(_direction, _channels.at(chan));
}

/*******************************************************************
 * Gain
 ******************************************************************/
void SoapyBlock::setGain(const double gain)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setGainChan(i, gain);
}

void SoapyBlock::setGainMap(const Pothos::ObjectMap &gain)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setGainChanMap(i, gain);
}

void SoapyBlock::setGains(const Pothos::ObjectVector &gains)
{
    check_device_ptr();
    for (size_t i = 0; i < gains.size(); i++)
    {
        if (gains[i].canConvert(typeid(Pothos::ObjectMap))) this->setGainChanMap(i, gains[i].convert<Pothos::ObjectMap>());
        else this->setGainChan(i, gains[i].convert<double>());
    }
}

void SoapyBlock::setGainName(const size_t chan, const std::string &name, const double gain)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setGain(_direction, _channels.at(chan), name, gain);
}

double SoapyBlock::getGainName(const size_t chan, const std::string &name) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getGain(_direction, _channels.at(chan), name);
}

void SoapyBlock::setGainChan(const size_t chan, const double gain)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setGain(_direction, _channels.at(chan), gain);
}

double SoapyBlock::getGain(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getGain(_direction, _channels.at(chan));
}

void SoapyBlock::setGainChanMap(const size_t chan, const Pothos::ObjectMap &args)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    for (const auto &pair : args)
    {
        const auto name = pair.first.convert<std::string>();
        const auto gain = pair.second.convert<double>();
        _device->setGain(_direction, _channels.at(chan), name, gain);
    }
}

std::vector<std::string> SoapyBlock::getGainNames(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return std::vector<std::string>();
    return _device->listGains(_direction, _channels.at(chan));
}

/*******************************************************************
 * Antennas
 ******************************************************************/
void SoapyBlock::setAntenna(const std::string &name)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setAntennaChan(i, name);
}

void SoapyBlock::setAntennas(const std::vector<std::string> &names)
{
    check_device_ptr();
    for (size_t i = 0; i < names.size(); i++) this->setAntennaChan(i, names[i]);
}

void SoapyBlock::setAntennaChan(const size_t chan, const std::string &name)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    if (name.empty()) return;
    return _device->setAntenna(_direction, _channels.at(chan), name);
}

std::string SoapyBlock::getAntenna(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return "";
    return _device->getAntenna(_direction, _channels.at(chan));
}

std::vector<std::string> SoapyBlock::getAntennas(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return std::vector<std::string>();
    return _device->listAntennas(_direction, _channels.at(chan));
}

/*******************************************************************
 * Bandwidth
 ******************************************************************/
void SoapyBlock::setBandwidth(const double bandwidth)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setBandwidthChan(i, bandwidth);
}

void SoapyBlock::setBandwidths(const std::vector<double> &bandwidths)
{
    check_device_ptr();
    for (size_t i = 0; i < bandwidths.size(); i++) this->setBandwidthChan(i, bandwidths[i]);
}

void SoapyBlock::setBandwidthChan(const size_t chan, const double bandwidth)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    if (bandwidth == 0) return;
    return _device->setBandwidth(_direction, _channels.at(chan), bandwidth);
}

double SoapyBlock::getBandwidth(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getBandwidth(_direction, _channels.at(chan));
}

std::vector<double> SoapyBlock::getBandwidths(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return std::vector<double>();
    return _device->listBandwidths(_direction, _channels.at(chan));
}

/*******************************************************************
 * DC offset mode
 ******************************************************************/
void SoapyBlock::setDCOffsetMode(const bool automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setDCOffsetModeChan(i, automatic);
}

void SoapyBlock::setDCOffsetModes(const std::vector<bool> &automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < automatic.size(); i++) this->setDCOffsetModeChan(i, automatic[i]);
}

void SoapyBlock::setDCOffsetModeChan(const size_t chan, const bool automatic)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setDCOffsetMode(_direction, _channels.at(chan), automatic);
}

bool SoapyBlock::getDCOffsetMode(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getDCOffsetMode(_direction, _channels.at(chan));
}

/*******************************************************************
 * DC offset adjust
 ******************************************************************/
void SoapyBlock::setDCOffsetAdjust(const std::complex<double> &correction)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setDCOffsetAdjustChan(i, correction);
}

void SoapyBlock::setDCOffsetAdjustChan(const size_t chan, const std::complex<double> &correction)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setDCOffset(_direction, _channels.at(chan), correction);
}

std::complex<double> SoapyBlock::getDCOffsetAdjust(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getDCOffset(_direction, _channels.at(chan));
}

/*******************************************************************
 * Clocking config
 ******************************************************************/
void SoapyBlock::setClockRate(const double rate)
{
    check_device_ptr();
    if (rate == 0.0) return;
    return _device->setMasterClockRate(rate);
}

double SoapyBlock::getClockRate(void) const
{
    check_device_ptr();
    return _device->getMasterClockRate();
}

void SoapyBlock::setClockSource(const std::string &source)
{
    check_device_ptr();
    if (source.empty()) return;
    return _device->setClockSource(source);
}

std::string SoapyBlock::getClockSource(void) const
{
    check_device_ptr();
    return _device->getClockSource();
}

std::vector<std::string> SoapyBlock::getClockSources(void) const
{
    check_device_ptr();
    return _device->listClockSources();
}

/*******************************************************************
 * Timing
 ******************************************************************/
void SoapyBlock::setTimeSource(const std::string &source)
{
    check_device_ptr();
    if (source.empty()) return;
    return _device->setTimeSource(source);
}

std::string SoapyBlock::getTimeSource(void) const
{
    check_device_ptr();
    return _device->getTimeSource();
}

std::vector<std::string> SoapyBlock::getTimeSources(void) const
{
    check_device_ptr();
    return _device->listTimeSources();
}

void SoapyBlock::setHardwareTime(const long long timeNs, const std::string &what)
{
    check_device_ptr();
    return _device->setHardwareTime(timeNs, what);
}

long long SoapyBlock::getHardwareTime(const std::string &what) const
{
    check_device_ptr();
    return _device->getHardwareTime(what);
}

void SoapyBlock::setCommandTime(const long long timeNs)
{
    check_device_ptr();
    static bool once = false;
    if (not once)
    {
        once = true;
        poco_warning(_logger, "SoapyBlock::setCommandTime() deprecated, use setHardwareTime()");
    }
    return _device->setCommandTime(timeNs);
}

/*******************************************************************
 * Sensors
 ******************************************************************/
std::vector<std::string> SoapyBlock::getSensors(void) const
{
    check_device_ptr();
    return _device->listSensors();
}

std::string SoapyBlock::getSensor(const std::string &name) const
{
    check_device_ptr();
    return _device->readSensor(name);
}

std::vector<std::string> SoapyBlock::getSensorsChan(const size_t chan) const
{
    check_device_ptr();
    return _device->listSensors(_direction, chan);
}

std::string SoapyBlock::getSensorChan(const size_t chan, const std::string &name) const
{
    check_device_ptr();
    return _device->readSensor(_direction, chan, name);
}

/*******************************************************************
 * GPIO
 ******************************************************************/
std::vector<std::string> SoapyBlock::getGpioBanks(void) const
{
    check_device_ptr();
    return _device->listGPIOBanks();
}

void SoapyBlock::setGpioConfig(const Pothos::ObjectKwargs &config)
{
    check_device_ptr();
    if (config.empty()) return; //empty configs ok

    const auto bankIt = config.find("bank");
    const auto dirIt = config.find("dir");
    const auto maskIt = config.find("mask");
    const auto valueIt = config.find("value");

    //check and extract bank
    if (bankIt == config.end()) throw Pothos::InvalidArgumentException(
        "SoapyBlock::setGpioConfig()", "bank name missing");
    const auto bank = bankIt->second.convert<std::string>();

    //set data direction without mask
    if (dirIt != config.end() and maskIt == config.end())
    {
        _device->writeGPIODir(bank, dirIt->second.convert<unsigned>());
    }

    //set data direction with mask
    if (dirIt != config.end() and maskIt != config.end())
    {
        _device->writeGPIODir(bank, dirIt->second.convert<unsigned>(), maskIt->second.convert<unsigned>());
    }

    //set GPIO value without mask
    if (valueIt != config.end() and maskIt == config.end())
    {
        _device->writeGPIO(bank, valueIt->second.convert<unsigned>());
    }

    //set GPIO value with mask
    if (valueIt != config.end() and maskIt != config.end())
    {
        _device->writeGPIO(bank, valueIt->second.convert<unsigned>(), maskIt->second.convert<unsigned>());
    }
}

void SoapyBlock::setGpioConfigs(const Pothos::ObjectVector &config)
{
    check_device_ptr();
    for (const auto &entry : config)
    {
        if (not entry.canConvert(typeid(Pothos::ObjectKwargs)))
            throw Pothos::InvalidArgumentException(
            "SoapyBlock::setGpioConfig()", "invalid list entry");
        this->setGpioConfig(entry.convert<Pothos::ObjectKwargs>());
    }
}

unsigned SoapyBlock::getGpioValue(const std::string &bank) const
{
    check_device_ptr();
    return _device->readGPIO(bank);
}

/*******************************************************************
 * Settings
 ******************************************************************/

void SoapyBlock::setGlobalSettings(const Pothos::ObjectKwargs &config)
{
    check_device_ptr();
    for (const auto &pair : config)
    {
        this->setGlobalSetting(pair.first, pair.second);
    }
}

void SoapyBlock::setChannelSettingsArgs(const Pothos::ObjectKwargs &config)
{
    check_device_ptr();
    for (const auto &pair : config)
    {
        for (size_t i = 0; i < _channels.size(); i++)
        {
            this->setChannelSettingChan(i, pair.first, pair.second);
        }
    }
}

void SoapyBlock::setChannelSettings(const Pothos::ObjectVector &config)
{
    check_device_ptr();
    for (size_t i = 0; i < config.size(); i++)
    {
        const auto &config_i = config.at(i).convert<Pothos::ObjectKwargs>();
        for (const auto &pair : config_i)
        {
            this->setChannelSettingChan(i, pair.first, pair.second);
        }
    }
}

void SoapyBlock::setGlobalSetting(const std::string &key, const Pothos::Object &value)
{
    check_device_ptr();
    _device->writeSetting(key, _toString(value));
}

void SoapyBlock::setChannelSetting(const std::string &key, const Pothos::Object &value)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setChannelSettingChan(i, key, value);
}

void SoapyBlock::setChannelSettingChan(const size_t chan, const std::string &key, const Pothos::Object &value)
{
    check_device_ptr();
    _device->writeSetting(_direction, _channels.at(chan), key, _toString(value));
}

/*******************************************************************
 * Streaming implementation
 ******************************************************************/
void SoapyBlock::emitActivationSignals(void)
{
    this->emitSignal("getSampleRateTriggered", this->getSampleRate());
    this->emitSignal("getSampleRatesTriggered", this->getSampleRates());
    this->emitSignal("getFrontendMapTriggered", this->getFrontendMap());
    this->emitSignal("getClockRateTriggered", this->getClockRate());
    this->emitSignal("getClockSourceTriggered", this->getClockSource());
    this->emitSignal("getClockSourcesTriggered", this->getClockSources());
    this->emitSignal("getTimeSourceTriggered", this->getTimeSource());
    this->emitSignal("getTimeSourcesTriggered", this->getTimeSources());
    this->emitSignal("getHardwareTimeTriggered", this->getHardwareTime());
    this->emitSignal("getSensorsTriggered", this->getSensors());
    this->emitSignal("getGpioBanksTriggered", this->getGpioBanks());
    for (size_t i = 0; i < _channels.size(); i++)
    {
        const auto chanStr = std::to_string(i);
        this->emitSignal("getFrequency"+chanStr+"Triggered", this->getFrequency(i));
        this->emitSignal("getGain"+chanStr+"Triggered", this->getGain(i));
        this->emitSignal("getGainNames"+chanStr+"Triggered", this->getGainNames(i));
        this->emitSignal("getGainMode"+chanStr+"Triggered", this->getGainMode(i));
        this->emitSignal("getAntenna"+chanStr+"Triggered", this->getAntenna(i));
        this->emitSignal("getAntennas"+chanStr+"Triggered", this->getAntennas(i));
        this->emitSignal("getBandwidth"+chanStr+"Triggered", this->getBandwidth(i));
        this->emitSignal("getBandwidths"+chanStr+"Triggered", this->getBandwidths(i));
        this->emitSignal("getDCOffsetMode"+chanStr+"Triggered", this->getDCOffsetMode(i));
    }
}

void SoapyBlock::activate(void)
{
    if (not this->isReady()) throw Pothos::Exception("SDRSource::activate()", "device not ready");

    check_stream_ptr();

    if (_autoActivate)
    {
        int ret = 0;
        //use time alignment for multi-channel rx setup when hardware time is supported
        if (_device->hasHardwareTime() and _direction == SOAPY_SDR_RX)
        {
            const auto delta = long(1e9*0.05); //50ms in the future for time-aligned streaming
            ret = _device->activateStream(_stream, SOAPY_SDR_HAS_TIME, _device->getHardwareTime()+delta);
            if (ret == SOAPY_SDR_NOT_SUPPORTED) ret = _device->activateStream(_stream); //try again, no time
        }
        else
        {
            ret = _device->activateStream(_stream);
        }
        if (ret != 0) throw Pothos::Exception("SoapyBlock::activate()", "activateStream returned " + std::string(SoapySDR::errToStr(ret)));
    }

    this->emitActivationSignals();

    //status forwarder start
    this->configureStatusThread();
}

void SoapyBlock::deactivate(void)
{
    //status forwarder shutdown
    this->configureStatusThread();

    const int ret = _device->deactivateStream(_stream);
    if (ret != 0) throw Pothos::Exception("SoapyBlock::deactivate()", "deactivateStream returned " + std::string(SoapySDR::errToStr(ret)));
}
