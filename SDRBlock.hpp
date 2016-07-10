// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <Pothos/Object/Containers.hpp>
#include <SoapySDR/Device.hpp>
#include <future>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

class SDRBlock : public Pothos::Block
{
public:
    SDRBlock(const int direction, const Pothos::DType &dtype, const std::vector<size_t> &channels);
    virtual ~SDRBlock(void);

    /*******************************************************************
     * Device object creation
     ******************************************************************/
    void setupDevice(const Pothos::ObjectKwargs &deviceArgs);

    /*******************************************************************
     * Delayed method dispatch
     ******************************************************************/

    //! Evaluate setters in a background thread as to not block
    void setCallingMode(const std::string &mode);

    //! Once activated, allow settings to queue and discard old ones
    void setEventSquash(const bool enable);

    Pothos::Object opaqueCallHandler(const std::string &name, const Pothos::Object *inputArgs, const size_t numArgs);

    /*******************************************************************
     * Stream config
     ******************************************************************/
    void setupStream(const Pothos::ObjectKwargs &streamArgs);

    void setSampleRate(const double rate);

    double getSampleRate(void) const;

    std::vector<double> getSampleRates(void) const;

    void setAutoActivate(const bool autoActivate);

    void streamControl(const std::string &what, const long long timeNs, const size_t numElems);

    void setEnableStatus(const bool enable);

    void forwardStatusLoop(void);

    void configureStatusThread(void);

    /*******************************************************************
     * Frontend map
     ******************************************************************/
    void setFrontendMap(const std::string &mapping);

    std::string getFrontendMap(void) const;

    /*******************************************************************
     * Frequency
     ******************************************************************/

    //-------- setFrequency(no tune args) ----------//

    void setFrequency(const double freq);

    void setFrequencies(const std::vector<double> &freqs);

    void setFrequencyChan(const size_t chan, const double freq);

    void setFrequencyName(const size_t chan, const std::string &name, const double freq);

    //-------- setFrequency(tune args) ----------//

    void setFrequencyArgs(const double freq, const Pothos::ObjectKwargs &args);

    void setFrequenciesArgs(const std::vector<double> &freqs, const Pothos::ObjectKwargs &args);

    void setFrequencyChanArgs(const size_t chan, const double freq, const Pothos::ObjectKwargs &args);

    void setFrequencyNameArgs(const size_t chan, const std::string &name, const double freq, const Pothos::ObjectKwargs &args);

    //-------- getFrequency ----------//

    double getFrequency(const size_t chan) const;

    double getFrequencyChan(const size_t chan, const std::string &name) const;

    /*******************************************************************
     * Gain mode
     ******************************************************************/
    void setGainMode(const bool automatic);

    void setGainModes(const std::vector<bool> &automatic);

    void setGainModeChan(const size_t chan, const bool automatic);

    double getGainMode(const size_t chan) const;

    /*******************************************************************
     * Gain
     ******************************************************************/
    void setGain(const double gain);

    void setGainMap(const Pothos::ObjectMap &gain);

    void setGains(const Pothos::ObjectVector &gains);

    void setGainName(const size_t chan, const std::string &name, const double gain);

    double getGainName(const size_t chan, const std::string &name) const;

    void setGainChan(const size_t chan, const double gain);

    double getGain(const size_t chan) const;

    void setGainChanMap(const size_t chan, const Pothos::ObjectMap &args);

    std::vector<std::string> getGainNames(const size_t chan) const;

    /*******************************************************************
     * Antennas
     ******************************************************************/
    void setAntenna(const std::string &name);

    void setAntennas(const std::vector<std::string> &names);

    void setAntennaChan(const size_t chan, const std::string &name);

    std::string getAntenna(const size_t chan) const;

    std::vector<std::string> getAntennas(const size_t chan) const;

    /*******************************************************************
     * Bandwidth
     ******************************************************************/
    void setBandwidth(const double bandwidth);

    void setBandwidths(const std::vector<double> &bandwidths);

    void setBandwidthChan(const size_t chan, const double bandwidth);

    double getBandwidth(const size_t chan) const;

    std::vector<double> getBandwidths(const size_t chan) const;

    /*******************************************************************
     * DC offset mode
     ******************************************************************/
    void setDCOffsetMode(const bool automatic);

    void setDCOffsetModes(const std::vector<bool> &automatic);

    void setDCOffsetModeChan(const size_t chan, const bool automatic);

    bool getDCOffsetMode(const size_t chan) const;

    /*******************************************************************
     * DC offset adjust
     ******************************************************************/
    void setDCOffsetAdjust(const std::complex<double> &correction);

    void setDCOffsetAdjustChan(const size_t chan, const std::complex<double> &correction);

    std::complex<double> getDCOffsetAdjust(const size_t chan) const;

    /*******************************************************************
     * Clocking config
     ******************************************************************/
    void setClockRate(const double rate);

    double getClockRate(void) const;

    void setClockSource(const std::string &source);

    std::string getClockSource(void) const;

    std::vector<std::string> getClockSources(void) const;

    /*******************************************************************
     * Timing
     ******************************************************************/
    void setTimeSource(const std::string &source);

    std::string getTimeSource(void) const;

    std::vector<std::string> getTimeSources(void) const;

    void setHardwareTime(const long long timeNs, const std::string &what = "");

    long long getHardwareTime(const std::string &what = "") const;

    void setCommandTime(const long long timeNs);

    /*******************************************************************
     * Sensors
     ******************************************************************/
    std::vector<std::string> getSensors(void) const;

    std::string getSensor(const std::string &name) const;

    std::vector<std::string> getSensorsChan(const size_t chan) const;

    std::string getSensorChan(const size_t chan, const std::string &name) const;

    /*******************************************************************
     * GPIO
     ******************************************************************/
    std::vector<std::string> getGpioBanks(void) const;

    void setGpioConfig(const Pothos::ObjectKwargs &config);

    void setGpioConfigs(const Pothos::ObjectVector &config);

    unsigned getGpioValue(const std::string &bank) const;

    /*******************************************************************
     * Settings
     ******************************************************************/

    void setGlobalSettings(const Pothos::ObjectKwargs &config);

    void setChannelSettingsArgs(const Pothos::ObjectKwargs &config);

    //vector of kwargs version for each channel
    void setChannelSettings(const Pothos::ObjectVector &config);

    //--- versions below for single settings ---//

    //write specific key for a global setting
    void setGlobalSetting(const std::string &key, const Pothos::Object &value);

    //write specific key to all channels with this block
    void setChannelSetting(const std::string &key, const Pothos::Object &value);

    //write specific key to specific channel
    void setChannelSettingChan(const size_t chan, const std::string &key, const Pothos::Object &value);

    /*******************************************************************
     * Streaming implementation
     ******************************************************************/
    virtual void activate(void);
    virtual void deactivate(void);
    virtual void work(void) = 0;

private:
    std::string _toString(const Pothos::Object &val)
    {
        if (val.type() == typeid(std::string)) return val.extract<std::string>();
        return val.toString();
    }

    SoapySDR::Kwargs _toKwargs(const Pothos::ObjectKwargs &args)
    {
        SoapySDR::Kwargs kwargs;
        for (const auto &pair : args)
        {
            kwargs[pair.first] = _toString(pair.second);
        }
        return kwargs;
    }

protected:
    bool isReady(void);
    void emitActivationSignals(void);

    bool _backgrounding;
    bool _activateWaits;
    bool _eventSquash;
    bool _autoActivate;
    const int _direction;
    const Pothos::DType _dtype;
    const std::vector<size_t> _channels;
    SoapySDR::Device *_device;
    SoapySDR::Stream *_stream;

    bool _enableStatus;
    std::thread _statusMonitor;

    //evaluation thread
    std::mutex _argsMutex;
    std::condition_variable _cond;
    std::vector<std::pair<std::string, Pothos::ObjectVector>> _cachedArgs;
    std::thread _evalThread;
    void evalThreadLoop(void);
    std::exception_ptr _evalError;
    std::atomic<bool> _evalThreadDone;
    std::atomic<bool> _evalErrorValid;
    std::shared_future<SoapySDR::Device *> _deviceFuture;

    std::vector<Pothos::ObjectKwargs> _pendingLabels;

    //Save the last tune args to re-use when slots are called without args.
    //This means that args can be set once at initialization and re-used.
    std::map<size_t, Pothos::ObjectKwargs> _cachedTuneArgs;
};
