// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <PothosSDR/Interface.hpp>

std::vector<std::string> PothosSDR::SDRDevice::getRxChannels(void) const
{
    return std::vector<std::string>();
}

std::vector<std::string> PothosSDR::SDRDevice::getTxChannels(void) const
{
    return std::vector<std::string>();
}

std::vector<std::string> PothosSDR::SDRDevice::listAntennas(const std::string &) const
{
    return std::vector<std::string>();
}

void PothosSDR::SDRDevice::setAntenna(const std::string &, const std::string &)
{
    return;
}

std::string PothosSDR::SDRDevice::getAntenna(const std::string &) const
{
    return "";
}

void PothosSDR::SDRDevice::setDCOffset(const std::string &, const std::complex<double> &)
{
    return;
}

std::complex<double> PothosSDR::SDRDevice::getDCOffset(const std::string &) const
{
    return std::complex<double>();
}

void PothosSDR::SDRDevice::setIQBalance(const std::string &, const std::complex<double> &)
{
    return;
}

std::complex<double> PothosSDR::SDRDevice::getIQBalance(const std::string &) const
{
    return std::complex<double>();
}

std::vector<std::string> PothosSDR::SDRDevice::listGains(const std::string &) const
{
    return std::vector<std::string>();
}

void PothosSDR::SDRDevice::setGainMode(const std::string &, const bool)
{
    return;
}

bool PothosSDR::SDRDevice::getGainMode(const std::string &channel) const
{
    return false;
}

void PothosSDR::SDRDevice::setGain(const std::string &, const std::string &, const double)
{
    return;
}

void PothosSDR::SDRDevice::setGains(const std::string &, const NumericDict &)
{
    return;
}

double PothosSDR::SDRDevice::getGainValue(const std::string &, const std::string &) const
{
    return 0.0;
}

PothosSDR::NumericDict PothosSDR::SDRDevice::getGainValues(const std::string &) const
{
    return PothosSDR::NumericDict();
}

PothosSDR::RangeList PothosSDR::SDRDevice::getGainRange(const std::string &, const std::string &) const
{
    return PothosSDR::RangeList();
}

void PothosSDR::SDRDevice::setFrequency(const std::string &, const double, const Kwargs &)
{
    return;
}

void PothosSDR::SDRDevice::setFrequency(const std::string &, const NumericDict &, const Kwargs &)
{
    return;
}

double PothosSDR::SDRDevice::getFrequency(const std::string &) const
{
    return 0.0;
}

PothosSDR::NumericDict PothosSDR::SDRDevice::getFrequencyComponents(const std::string &) const
{
    return PothosSDR::NumericDict();
}

PothosSDR::RangeList PothosSDR::SDRDevice::getFrequencyRange(const std::string &) const
{
    return PothosSDR::RangeList();
}

void PothosSDR::SDRDevice::setSampleRate(const std::string &, const double)
{
    return;
}

double PothosSDR::SDRDevice::getSampleRate(const std::string &) const
{
    return 0.0;
}

PothosSDR::RangeList PothosSDR::SDRDevice::getSampleRateRange(const std::string &) const
{
    return PothosSDR::RangeList();
}

void PothosSDR::SDRDevice::setBandwidth(const std::string &, const double)
{
    return;
}

double PothosSDR::SDRDevice::getBandwidth(const std::string &) const
{
    return 0.0;
}

PothosSDR::RangeList PothosSDR::SDRDevice::getBandwidthRange(const std::string &) const
{
    return PothosSDR::RangeList();
}

void PothosSDR::SDRDevice::setMasterClockRate(const double)
{
    return;
}

double PothosSDR::SDRDevice::setMasterClockRate(void) const
{
    return 0.0;
}

std::vector<std::string> PothosSDR::SDRDevice::listClockSources(void) const
{
    return std::vector<std::string>();
}

void PothosSDR::SDRDevice::setClockSource(const std::string &)
{
    return;
}

std::string PothosSDR::SDRDevice::getClockSource(void) const
{
    return "";
}

std::vector<std::string> PothosSDR::SDRDevice::listTimeSources(void) const
{
    return std::vector<std::string>();
}

void PothosSDR::SDRDevice::setTimeSource(const std::string &)
{
    return;
}

std::string PothosSDR::SDRDevice::getTimeSource(void) const
{
    return "";
}
