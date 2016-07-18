// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SDRBlock.hpp"
#include <SoapySDR/Version.hpp>
#ifdef SOAPY_SDR_API_HAS_ERR_TO_STR
#include <SoapySDR/Errors.hpp>
#endif //SOAPY_SDR_API_HAS_ERR_TO_STR
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/SingletonHolder.h>
#include <Poco/Format.h>
#include <cassert>

#ifdef _MSC_VER
#  define current_func() std::string(__FUNCTION__)
#else
#  define current_func() std::string(__PRETTY_FUNCTION__)
#endif

#define check_device_ptr() {if (_device == nullptr) \
    throw Pothos::NullPointerException(Poco::format("%s - device not setup!", current_func()));}

#define check_stream_ptr() {if (_stream == nullptr) \
    throw Pothos::NullPointerException(Poco::format("%s - stream not setup!", current_func()));}

SDRBlock::SDRBlock(const int direction, const Pothos::DType &dtype, const std::vector<size_t> &chs):
    _logger(Poco::Logger::get("SDRBlock")),
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
    if (SoapySDR::getABIVersion() != SOAPY_SDR_ABI_VERSION) throw Pothos::Exception("SDRBlock::make()",
        Poco::format("Failed ABI check. Pothos SDR %s. Soapy SDR %s. Rebuild the module.",
        std::string(SOAPY_SDR_ABI_VERSION), SoapySDR::getABIVersion()));

    //hooks for overlay
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, overlay));

    //threading options
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setCallingMode));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setEventSquash));

    //streaming
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setupDevice));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setupStream));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setSampleRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getSampleRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getSampleRates));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setFrontendMap));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getFrontendMap));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setAutoActivate));
    this->registerCallable("streamControl", Pothos::Callable(&SDRBlock::streamControl).bind(std::ref(*this), 0)); //3 arg version
    this->registerCallable("streamControl", Pothos::Callable(&SDRBlock::streamControl).bind(std::ref(*this), 0).bind(0, 3)); //2 arg version
    this->registerCallable("streamControl", Pothos::Callable(&SDRBlock::streamControl).bind(std::ref(*this), 0).bind(0, 2).bind(0, 3)); //1 arg version
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setEnableStatus));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setGlobalSettings));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setGlobalSetting));

    //channels -- called by setters
    this->registerCall(this, "setFrequency",       &SDRBlock::setFrequency);
    this->registerCall(this, "setFrequency",       &SDRBlock::setFrequencyArgs);
    this->registerCall(this, "setFrequency",       &SDRBlock::setFrequencies);
    this->registerCall(this, "setFrequency",       &SDRBlock::setFrequenciesArgs);
    this->registerCall(this, "setGainMode",        &SDRBlock::setGainMode);
    this->registerCall(this, "setGainMode",        &SDRBlock::setGainModes);
    this->registerCall(this, "setGain",            &SDRBlock::setGain);
    this->registerCall(this, "setGain",            &SDRBlock::setGainMap);
    this->registerCall(this, "setGain",            &SDRBlock::setGains);
    this->registerCall(this, "setAntenna",         &SDRBlock::setAntenna);
    this->registerCall(this, "setAntenna",         &SDRBlock::setAntennas);
    this->registerCall(this, "setBandwidth",       &SDRBlock::setBandwidth);
    this->registerCall(this, "setBandwidth",       &SDRBlock::setBandwidths);
    this->registerCall(this, "setDCOffsetMode",    &SDRBlock::setDCOffsetMode);
    this->registerCall(this, "setDCOffsetMode",    &SDRBlock::setDCOffsetModes);
    this->registerCall(this, "setDCOffsetAdjust",  &SDRBlock::setDCOffsetAdjust);
    this->registerCall(this, "setChannelSettings", &SDRBlock::setChannelSettings);
    this->registerCall(this, "setChannelSettings", &SDRBlock::setChannelSettingsArgs);
    this->registerCall(this, "setChannelSetting",  &SDRBlock::setChannelSetting);

    //channels
    for (size_t i = 0; i < _channels.size(); i++)
    {
        const auto chanStr = std::to_string(i);
        //freq overall with tune args
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SDRBlock::setFrequencyChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SDRBlock::setFrequencyChanArgs).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getFrequency"+chanStr, Pothos::Callable(&SDRBlock::getFrequency).bind(std::ref(*this), 0).bind(i, 1));
        //freq component by name
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SDRBlock::setFrequencyName).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("setFrequency"+chanStr, Pothos::Callable(&SDRBlock::setFrequencyNameArgs).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getFrequency"+chanStr, Pothos::Callable(&SDRBlock::getFrequencyChan).bind(std::ref(*this), 0).bind(i, 1));
        //gain by name
        this->registerCallable("setGain"+chanStr, Pothos::Callable(&SDRBlock::setGainName).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGain"+chanStr, Pothos::Callable(&SDRBlock::getGainName).bind(std::ref(*this), 0).bind(i, 1));
        //gain overall
        this->registerCallable("setGain"+chanStr, Pothos::Callable(&SDRBlock::setGainChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGain"+chanStr, Pothos::Callable(&SDRBlock::getGain).bind(std::ref(*this), 0).bind(i, 1));
        //gain dict
        this->registerCallable("setGain"+chanStr, Pothos::Callable(&SDRBlock::setGainChanMap).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGainNames"+chanStr, Pothos::Callable(&SDRBlock::getGainNames).bind(std::ref(*this), 0).bind(i, 1));
        //gain mode
        this->registerCallable("setGainMode"+chanStr, Pothos::Callable(&SDRBlock::setGainModeChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getGainMode"+chanStr, Pothos::Callable(&SDRBlock::getGainMode).bind(std::ref(*this), 0).bind(i, 1));
        //antenna
        this->registerCallable("setAntenna"+chanStr, Pothos::Callable(&SDRBlock::setAntennaChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getAntenna"+chanStr, Pothos::Callable(&SDRBlock::getAntenna).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getAntennas"+chanStr, Pothos::Callable(&SDRBlock::getAntennas).bind(std::ref(*this), 0).bind(i, 1));
        //bandwidth
        this->registerCallable("setBandwidth"+chanStr, Pothos::Callable(&SDRBlock::setBandwidthChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getBandwidth"+chanStr, Pothos::Callable(&SDRBlock::getBandwidth).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getBandwidths"+chanStr, Pothos::Callable(&SDRBlock::getBandwidths).bind(std::ref(*this), 0).bind(i, 1));
        //dc offset mode
        this->registerCallable("setDCOffsetMode"+chanStr, Pothos::Callable(&SDRBlock::setDCOffsetModeChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getDCOffsetMode"+chanStr, Pothos::Callable(&SDRBlock::getDCOffsetMode).bind(std::ref(*this), 0).bind(i, 1));
        //dc offset adjust
        this->registerCallable("setDCOffsetAdjust"+chanStr, Pothos::Callable(&SDRBlock::setDCOffsetAdjustChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getDCOffsetAdjust"+chanStr, Pothos::Callable(&SDRBlock::getDCOffsetAdjust).bind(std::ref(*this), 0).bind(i, 1));
        //sensors
        this->registerCallable("getSensors"+chanStr, Pothos::Callable(&SDRBlock::getSensorsChan).bind(std::ref(*this), 0).bind(i, 1));
        this->registerCallable("getSensor"+chanStr, Pothos::Callable(&SDRBlock::getSensorChan).bind(std::ref(*this), 0).bind(i, 1));
        //settings
        this->registerCallable("setChannelSetting"+chanStr, Pothos::Callable(&SDRBlock::setChannelSettingChan).bind(std::ref(*this), 0).bind(i, 1));

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
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setClockRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getClockRate));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setClockSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getClockSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getClockSources));

    //time
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setTimeSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getTimeSource));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getTimeSources));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setHardwareTime));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getHardwareTime));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setCommandTime));
    this->registerCallable("setHardwareTime", Pothos::Callable(&SDRBlock::setHardwareTime).bind(std::ref(*this), 0).bind(std::string(), 2));
    this->registerCallable("getHardwareTime", Pothos::Callable(&SDRBlock::getHardwareTime).bind(std::ref(*this), 0).bind(std::string(), 1));

    //sensors
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getSensors));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getSensor));

    //gpio
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getGpioBanks));
    this->registerCallable("setGpioConfig", Pothos::Callable(&SDRBlock::setGpioConfigs).bind(std::ref(*this), 0));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, setGpioConfig));
    this->registerCall(this, POTHOS_FCN_TUPLE(SDRBlock, getGpioValue));

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
    _evalThread = std::thread(&SDRBlock::evalThreadLoop, this);
}

static std::mutex &getMutex(void)
{
    static Poco::SingletonHolder<std::mutex> sh;
    return *sh.get();
}

std::string SDRBlock::overlay(void) const
{
    Poco::JSON::Object::Ptr topObj(new Poco::JSON::Object());

    Poco::JSON::Array::Ptr params(new Poco::JSON::Array());
    topObj->set("params", params);

    Poco::JSON::Object::Ptr deviceArgsParam(new Poco::JSON::Object());
    params->add(deviceArgsParam);

    Poco::JSON::Array::Ptr options(new Poco::JSON::Array());
    deviceArgsParam->set("key", "deviceArgs");
    deviceArgsParam->set("options", options);

    //editable drop down for user-controlled input
    Poco::JSON::Object::Ptr deviceArgsWidgetKwargs(new Poco::JSON::Object());
    deviceArgsWidgetKwargs->set("editable", true);
    deviceArgsParam->set("widgetKwargs", deviceArgsWidgetKwargs);
    deviceArgsParam->set("widgetType", "DropDown");

    //a default option for empty/unspecified device
    Poco::JSON::Object::Ptr defaultOption(new Poco::JSON::Object());
    defaultOption->set("name", "Null Device");
    defaultOption->set("value", "{\"driver\":\"null\"}");
    options->add(defaultOption);

    //protect device make -- its not thread safe
    std::unique_lock<std::mutex> lock(getMutex());

    //enumerate devices and add to the options list
    for (const auto &args : SoapySDR::Device::enumerate())
    {
        //create args dictionary
        Poco::JSON::Object argsObj;
        for (const auto &pair : args) argsObj.set(pair.first, pair.second);
        std::stringstream ss; argsObj.stringify(ss);

        //create displayable name
        std::vector<std::string> nameInfo;
        for (const auto &pair : args)
        {
            if (pair.first == "type" or
                pair.first == "serial" or
                pair.first == "addr" or
                pair.first == "host" or
                pair.first == "remote" or
                pair.first.find("id") != std::string::npos)
            {
                nameInfo.push_back(pair.first + "=" + pair.second);
            }
        }
        std::string name;
        for (const auto &info : nameInfo)
        {
            if (not name.empty()) name += ", ";
            name += info;
        }
        if (args.count("driver") != 0) name = args.at("driver") + "(" + name + ")";

        Poco::JSON::Object::Ptr option(new Poco::JSON::Object());
        option->set("name", name);
        option->set("value", ss.str());
        options->add(option);
    }

    std::stringstream ss;
    topObj->stringify(ss, 4);
    return ss.str();
}

void SDRBlock::setupDevice(const Pothos::ObjectKwargs &deviceArgs)
{
    //protect device make -- its not thread safe
    std::unique_lock<std::mutex> lock(getMutex());
    _device = SoapySDR::Device::make(_toKwargs(deviceArgs));
}

SDRBlock::~SDRBlock(void)
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

    //if for some reason we didn't complete the future
    //we have to wait on it here and catch all errors
    try {_device = _deviceFuture.get();} catch (...){}

    //now with the mutex locked, the device object can be released
    std::unique_lock<std::mutex> lock(getMutex());
    if (_device != nullptr) SoapySDR::Device::unmake(_device);
}

/*******************************************************************
 * Stream config
 ******************************************************************/
void SDRBlock::setupStream(const Pothos::ObjectKwargs &streamArgs)
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

void SDRBlock::setSampleRate(const double rate)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++)
    {
        _device->setSampleRate(_direction, _channels.at(i), rate);
        _pendingLabels[i]["rxRate"] = Pothos::Object(_device->getSampleRate(_direction, _channels.at(i)));
    }
}

double SDRBlock::getSampleRate(void) const
{
    check_device_ptr();
    return _device->getSampleRate(_direction, _channels.front());
}

std::vector<double> SDRBlock::getSampleRates(void) const
{
    check_device_ptr();
    return _device->listSampleRates(_direction, _channels.front());
}

void SDRBlock::setAutoActivate(const bool autoActivate)
{
    _autoActivate = autoActivate;
}

void SDRBlock::streamControl(const std::string &what, const long long timeNs, const size_t numElems)
{
    check_stream_ptr();
    int r = 0;
    if (what == "ACTIVATE")          r = _device->activateStream(_stream, 0, timeNs, numElems);
    if (what == "ACTIVATE_AT")       r = _device->activateStream(_stream, SOAPY_SDR_HAS_TIME, timeNs, numElems);
    if (what == "ACTIVATE_BURST")    r = _device->activateStream(_stream, SOAPY_SDR_END_BURST, timeNs, numElems);
    if (what == "ACTIVATE_BURST_AT") r = _device->activateStream(_stream, SOAPY_SDR_HAS_TIME | SOAPY_SDR_END_BURST, timeNs, numElems);
    if (what == "DEACTIVATE")        r = _device->deactivateStream(_stream, 0, timeNs);
    if (what == "DEACTIVATE_AT")     r = _device->deactivateStream(_stream, SOAPY_SDR_HAS_TIME, timeNs);
    if (r != 0) throw Pothos::Exception("SDRBlock::streamControl("+what+")", "de/activateStream returned " + std::to_string(r));
}

void SDRBlock::setEnableStatus(const bool enable)
{
    _enableStatus = enable;
    this->configureStatusThread();
}

void SDRBlock::forwardStatusLoop(void)
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
        #ifdef SOAPY_SDR_API_HAS_ERR_TO_STR
        if (ret != 0) status["error"] = Pothos::Object(SoapySDR::errToStr(ret));
        #endif //SOAPY_SDR_API_HAS_ERR_TO_STR

        //emit the status signal
        this->emitSignal("status", status);

        //exit the thread if stream status is not supported
        //but only after reporting this to "status" signal
        if (ret == SOAPY_SDR_NOT_SUPPORTED) return;
    }
}

void SDRBlock::configureStatusThread(void)
{
    //ensure thread is running
    if (this->isActive() and _enableStatus)
    {
        if (_statusMonitor.joinable()) return;
        _statusMonitor = std::thread(&SDRBlock::forwardStatusLoop, this);
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
void SDRBlock::setFrontendMap(const std::string &mapping)
{
    check_device_ptr();
    if (mapping.empty()) return;
    return _device->setFrontendMapping(_direction, mapping);
}

std::string SDRBlock::getFrontendMap(void) const
{
    check_device_ptr();
    return _device->getFrontendMapping(_direction);
}

/*******************************************************************
 * Frequency
 ******************************************************************/

//-------- setFrequency(no tune args) ----------//

void SDRBlock::setFrequency(const double freq)
{
    check_device_ptr();
    this->setFrequencyArgs(freq, _cachedTuneArgs[0]);
}

void SDRBlock::setFrequencies(const std::vector<double> &freqs)
{
    check_device_ptr();
    this->setFrequenciesArgs(freqs, _cachedTuneArgs[0]);
}

void SDRBlock::setFrequencyChan(const size_t chan, const double freq)
{
    check_device_ptr();
    this->setFrequencyChanArgs(chan, freq, _cachedTuneArgs[chan]);
}

void SDRBlock::setFrequencyName(const size_t chan, const std::string &name, const double freq)
{
    check_device_ptr();
    this->setFrequencyNameArgs(chan, name, freq, _cachedTuneArgs[chan]);
}

//-------- setFrequency(tune args) ----------//

void SDRBlock::setFrequencyArgs(const double freq, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setFrequencyChanArgs(i, freq, args);
}

void SDRBlock::setFrequenciesArgs(const std::vector<double> &freqs, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    for (size_t i = 0; i < freqs.size(); i++) this->setFrequencyChanArgs(i, freqs[i], args);
}

void SDRBlock::setFrequencyChanArgs(const size_t chan, const double freq, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    _cachedTuneArgs[chan] = args;
    _device->setFrequency(_direction, _channels.at(chan), freq, _toKwargs(args));
    _pendingLabels[chan]["rxFreq"] = Pothos::Object(_device->getFrequency(_direction, _channels.at(chan)));
}

void SDRBlock::setFrequencyNameArgs(const size_t chan, const std::string &name, const double freq, const Pothos::ObjectKwargs &args)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    _cachedTuneArgs[chan] = args;
    _device->setFrequency(_direction, _channels.at(chan), name, freq, _toKwargs(args));
}

//-------- getFrequency ----------//

double SDRBlock::getFrequency(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getFrequency(_direction, _channels.at(chan));
}

double SDRBlock::getFrequencyChan(const size_t chan, const std::string &name) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getFrequency(_direction, _channels.at(chan), name);
}

/*******************************************************************
 * Gain mode
 ******************************************************************/
void SDRBlock::setGainMode(const bool automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setGainModeChan(i, automatic);
}

void SDRBlock::setGainModes(const std::vector<bool> &automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < automatic.size(); i++) this->setGainModeChan(i, automatic[i]);
}

void SDRBlock::setGainModeChan(const size_t chan, const bool automatic)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setGainMode(_direction, _channels.at(chan), automatic);
}

double SDRBlock::getGainMode(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return false;
    return _device->getGainMode(_direction, _channels.at(chan));
}

/*******************************************************************
 * Gain
 ******************************************************************/
void SDRBlock::setGain(const double gain)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setGainChan(i, gain);
}

void SDRBlock::setGainMap(const Pothos::ObjectMap &gain)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setGainChanMap(i, gain);
}

void SDRBlock::setGains(const Pothos::ObjectVector &gains)
{
    check_device_ptr();
    for (size_t i = 0; i < gains.size(); i++)
    {
        if (gains[i].canConvert(typeid(Pothos::ObjectMap))) this->setGainChanMap(i, gains[i].convert<Pothos::ObjectMap>());
        else this->setGainChan(i, gains[i].convert<double>());
    }
}

void SDRBlock::setGainName(const size_t chan, const std::string &name, const double gain)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setGain(_direction, _channels.at(chan), name, gain);
}

double SDRBlock::getGainName(const size_t chan, const std::string &name) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getGain(_direction, _channels.at(chan), name);
}

void SDRBlock::setGainChan(const size_t chan, const double gain)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setGain(_direction, _channels.at(chan), gain);
}

double SDRBlock::getGain(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getGain(_direction, _channels.at(chan));
}

void SDRBlock::setGainChanMap(const size_t chan, const Pothos::ObjectMap &args)
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

std::vector<std::string> SDRBlock::getGainNames(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return std::vector<std::string>();
    return _device->listGains(_direction, _channels.at(chan));
}

/*******************************************************************
 * Antennas
 ******************************************************************/
void SDRBlock::setAntenna(const std::string &name)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setAntennaChan(i, name);
}

void SDRBlock::setAntennas(const std::vector<std::string> &names)
{
    check_device_ptr();
    for (size_t i = 0; i < names.size(); i++) this->setAntennaChan(i, names[i]);
}

void SDRBlock::setAntennaChan(const size_t chan, const std::string &name)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    if (name.empty()) return;
    return _device->setAntenna(_direction, _channels.at(chan), name);
}

std::string SDRBlock::getAntenna(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return "";
    return _device->getAntenna(_direction, _channels.at(chan));
}

std::vector<std::string> SDRBlock::getAntennas(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return std::vector<std::string>();
    return _device->listAntennas(_direction, _channels.at(chan));
}

/*******************************************************************
 * Bandwidth
 ******************************************************************/
void SDRBlock::setBandwidth(const double bandwidth)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setBandwidthChan(i, bandwidth);
}

void SDRBlock::setBandwidths(const std::vector<double> &bandwidths)
{
    check_device_ptr();
    for (size_t i = 0; i < bandwidths.size(); i++) this->setBandwidthChan(i, bandwidths[i]);
}

void SDRBlock::setBandwidthChan(const size_t chan, const double bandwidth)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    if (bandwidth == 0) return;
    return _device->setBandwidth(_direction, _channels.at(chan), bandwidth);
}

double SDRBlock::getBandwidth(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getBandwidth(_direction, _channels.at(chan));
}

std::vector<double> SDRBlock::getBandwidths(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return std::vector<double>();
    return _device->listBandwidths(_direction, _channels.at(chan));
}

/*******************************************************************
 * DC offset mode
 ******************************************************************/
void SDRBlock::setDCOffsetMode(const bool automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setDCOffsetModeChan(i, automatic);
}

void SDRBlock::setDCOffsetModes(const std::vector<bool> &automatic)
{
    check_device_ptr();
    for (size_t i = 0; i < automatic.size(); i++) this->setDCOffsetModeChan(i, automatic[i]);
}

void SDRBlock::setDCOffsetModeChan(const size_t chan, const bool automatic)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setDCOffsetMode(_direction, _channels.at(chan), automatic);
}

bool SDRBlock::getDCOffsetMode(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getDCOffsetMode(_direction, _channels.at(chan));
}

/*******************************************************************
 * DC offset adjust
 ******************************************************************/
void SDRBlock::setDCOffsetAdjust(const std::complex<double> &correction)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setDCOffsetAdjustChan(i, correction);
}

void SDRBlock::setDCOffsetAdjustChan(const size_t chan, const std::complex<double> &correction)
{
    check_device_ptr();
    if (chan >= _channels.size()) return;
    return _device->setDCOffset(_direction, _channels.at(chan), correction);
}

std::complex<double> SDRBlock::getDCOffsetAdjust(const size_t chan) const
{
    check_device_ptr();
    if (chan >= _channels.size()) return 0.0;
    return _device->getDCOffset(_direction, _channels.at(chan));
}

/*******************************************************************
 * Clocking config
 ******************************************************************/
void SDRBlock::setClockRate(const double rate)
{
    check_device_ptr();
    if (rate == 0.0) return;
    return _device->setMasterClockRate(rate);
}

double SDRBlock::getClockRate(void) const
{
    check_device_ptr();
    return _device->getMasterClockRate();
}

void SDRBlock::setClockSource(const std::string &source)
{
    check_device_ptr();
    if (source.empty()) return;
    return _device->setClockSource(source);
}

std::string SDRBlock::getClockSource(void) const
{
    check_device_ptr();
    return _device->getClockSource();
}

std::vector<std::string> SDRBlock::getClockSources(void) const
{
    check_device_ptr();
    return _device->listClockSources();
}

/*******************************************************************
 * Timing
 ******************************************************************/
void SDRBlock::setTimeSource(const std::string &source)
{
    check_device_ptr();
    if (source.empty()) return;
    return _device->setTimeSource(source);
}

std::string SDRBlock::getTimeSource(void) const
{
    check_device_ptr();
    return _device->getTimeSource();
}

std::vector<std::string> SDRBlock::getTimeSources(void) const
{
    check_device_ptr();
    return _device->listTimeSources();
}

void SDRBlock::setHardwareTime(const long long timeNs, const std::string &what)
{
    check_device_ptr();
    return _device->setHardwareTime(timeNs, what);
}

long long SDRBlock::getHardwareTime(const std::string &what) const
{
    check_device_ptr();
    return _device->getHardwareTime(what);
}

void SDRBlock::setCommandTime(const long long timeNs)
{
    check_device_ptr();
    static bool once = false;
    if (not once)
    {
        once = true;
        poco_warning(_logger, "SDRBlock::setCommandTime() deprecated, use setHardwareTime()");
    }
    return _device->setCommandTime(timeNs);
}

/*******************************************************************
 * Sensors
 ******************************************************************/
std::vector<std::string> SDRBlock::getSensors(void) const
{
    check_device_ptr();
    return _device->listSensors();
}

std::string SDRBlock::getSensor(const std::string &name) const
{
    check_device_ptr();
    return _device->readSensor(name);
}

std::vector<std::string> SDRBlock::getSensorsChan(const size_t chan) const
{
    check_device_ptr();
    #ifdef SOAPY_SDR_API_HAS_CHANNEL_SENSORS
    return _device->listSensors(_direction, chan);
    #else
    return std::vector<std::string>();
    #endif
}

std::string SDRBlock::getSensorChan(const size_t chan, const std::string &name) const
{
    check_device_ptr();
    #ifdef SOAPY_SDR_API_HAS_CHANNEL_SENSORS
    return _device->readSensor(_direction, chan, name);
    #else
    return "";
    #endif
}

/*******************************************************************
 * GPIO
 ******************************************************************/
std::vector<std::string> SDRBlock::getGpioBanks(void) const
{
    check_device_ptr();
    return _device->listGPIOBanks();
}

void SDRBlock::setGpioConfig(const Pothos::ObjectKwargs &config)
{
    check_device_ptr();
    if (config.empty()) return; //empty configs ok

    const auto bankIt = config.find("bank");
    const auto dirIt = config.find("dir");
    const auto maskIt = config.find("mask");
    const auto valueIt = config.find("value");

    //check and extract bank
    if (bankIt == config.end()) throw Pothos::InvalidArgumentException(
        "SDRBlock::setGpioConfig()", "bank name missing");
    const auto bank = bankIt->second.convert<std::string>();

    #ifdef SOAPY_SDR_API_HAS_MASKED_GPIO

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

    #endif //SOAPY_SDR_API_HAS_MASKED_GPIO
}

void SDRBlock::setGpioConfigs(const Pothos::ObjectVector &config)
{
    check_device_ptr();
    for (const auto &entry : config)
    {
        if (not entry.canConvert(typeid(Pothos::ObjectKwargs)))
            throw Pothos::InvalidArgumentException(
            "SDRBlock::setGpioConfig()", "invalid list entry");
        this->setGpioConfig(entry.convert<Pothos::ObjectKwargs>());
    }
}

unsigned SDRBlock::getGpioValue(const std::string &bank) const
{
    check_device_ptr();
    return _device->readGPIO(bank);
}

/*******************************************************************
 * Settings
 ******************************************************************/

void SDRBlock::setGlobalSettings(const Pothos::ObjectKwargs &config)
{
    check_device_ptr();
    for (const auto &pair : config)
    {
        this->setGlobalSetting(pair.first, pair.second);
    }
}

void SDRBlock::setChannelSettingsArgs(const Pothos::ObjectKwargs &config)
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

void SDRBlock::setChannelSettings(const Pothos::ObjectVector &config)
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

void SDRBlock::setGlobalSetting(const std::string &key, const Pothos::Object &value)
{
    check_device_ptr();
    _device->writeSetting(key, _toString(value));
}

void SDRBlock::setChannelSetting(const std::string &key, const Pothos::Object &value)
{
    check_device_ptr();
    for (size_t i = 0; i < _channels.size(); i++) this->setChannelSettingChan(i, key, value);
}

void SDRBlock::setChannelSettingChan(const size_t chan, const std::string &key, const Pothos::Object &value)
{
    check_device_ptr();
    #ifdef SOAPY_SDR_API_HAS_CHANNEL_SETTINGS
    _device->writeSetting(_direction, _channels.at(chan), key, _toString(value));
    #endif //SOAPY_SDR_API_HAS_CHANNEL_SETTINGS
}

/*******************************************************************
 * Streaming implementation
 ******************************************************************/
void SDRBlock::emitActivationSignals(void)
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

void SDRBlock::activate(void)
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
        }
        else
        {
            ret = _device->activateStream(_stream);
        }
        #ifdef SOAPY_SDR_API_HAS_ERR_TO_STR
        if (ret != 0) throw Pothos::Exception("SDRBlock::activate()", "activateStream returned " + std::string(SoapySDR::errToStr(ret)));
        #else
        if (ret != 0) throw Pothos::Exception("SDRBlock::activate()", "activateStream returned " + std::to_string(ret));
        #endif
    }

    this->emitActivationSignals();

    //status forwarder start
    this->configureStatusThread();
}

void SDRBlock::deactivate(void)
{
    //status forwarder shutdown
    this->configureStatusThread();

    const int ret = _device->deactivateStream(_stream);
    #ifdef SOAPY_SDR_API_HAS_ERR_TO_STR
    if (ret != 0) throw Pothos::Exception("SDRBlock::deactivate()", "deactivateStream returned " + std::string(SoapySDR::errToStr(ret)));
    #else
    if (ret != 0) throw Pothos::Exception("SDRBlock::deactivate()", "deactivateStream returned " + std::to_string(ret));
    #endif
}
