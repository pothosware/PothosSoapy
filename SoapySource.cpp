// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyBlock.hpp"
#include <SoapySDR/Errors.hpp>

class SDRSource : public SoapyBlock
{
public:
    static Block *make(const Pothos::DType &dtype, const std::vector<size_t> &channels)
    {
        return new SDRSource(dtype, channels);
    }

    SDRSource(const Pothos::DType &dtype, const std::vector<size_t> &channels):
        SoapyBlock(SOAPY_SDR_RX, dtype, channels),
        _postTime(false)
    {
        for (size_t i = 0; i < _channels.size(); i++) this->setupOutput(i, dtype);
    }

    /*******************************************************************
     * Streaming implementation
     ******************************************************************/
    void activate(void)
    {
        SoapyBlock::activate();
        _postTime = true;
    }

    void work(void)
    {
        int flags = 0;
        long long timeNs = 0;
        const size_t numElems = this->workInfo().minOutElements;
        if (numElems == 0) return;
        const long timeoutUs = this->workInfo().maxTimeoutNs/1000;
        const auto &buffs = this->workInfo().outputPointers;

        //initial non-blocking read for all available samples that can fit into the buffer
        int ret = _device->readStream(_stream, buffs.data(), numElems, flags, timeNs, 0);

        //otherwise perform a blocking read on the single transfer unit size (in samples)
        if (ret == SOAPY_SDR_TIMEOUT or ret == 0)
        {
            const auto minNumElems = std::min(numElems, _device->getStreamMTU(_stream));
            ret = _device->readStream(_stream, buffs.data(), minNumElems, flags, timeNs, timeoutUs);
        }

        //handle error
        if (ret <= 0)
        {
            //consider this to mean that the HW produced size 0 transfer
            //the flags and time may be valid, but we are discarding here
            if (ret == 0) return this->yield();
            //got timeout? just call again
            if (ret == SOAPY_SDR_TIMEOUT) return this->yield();
            //got overflow? call again, discontinuity means repost time
            if (ret == SOAPY_SDR_OVERFLOW) _postTime = true;
            if (ret == SOAPY_SDR_OVERFLOW) return this->yield();
            //otherwise throw an exception with the error code
            throw Pothos::Exception("SDRSource::work()", "readStream "+std::string(SoapySDR::errToStr(ret)));
        }

        //handle packet mode when SOAPY_SDR_ONE_PACKET is specified
        //produce a packet with matching labels and pop the buffer
        if (_channels.size() <= 1 and (flags & SOAPY_SDR_ONE_PACKET) != 0)
        {
            auto outPort0 = this->output(0);

            //set the packet payload
            Pothos::Packet pkt;
            pkt.payload = outPort0->buffer();
            pkt.payload.setElements(ret);

            //turn flags into metadata and labels
            if ((flags & SOAPY_SDR_HAS_TIME) != 0)
            {
                pkt.metadata["rxTime"] = Pothos::Object(timeNs);
                pkt.labels.emplace_back("rxTime", timeNs, 0);
            }
            if ((flags & SOAPY_SDR_END_BURST) != 0)
            {
                pkt.metadata["rxEnd"] = Pothos::Object(true);
                pkt.labels.emplace_back("rxEnd", true, ret-1);
            }

            //consume buffer, produce message, done work()
            outPort0->popElements(ret);
            outPort0->postMessage(pkt);
            return;
        }

        //produce output and post pending labels
        for (auto output : this->outputs())
        {
            output->produce(size_t(ret));

            //pending rx configuration labels
            auto &pending = _pendingLabels.at(output->index());
            if (pending.empty()) continue;
            for (const auto &pair : pending)
            {
                output->postLabel(Pothos::Label(pair.first, pair.second, 0));
            }
            pending.clear();
        }

        //post labels from stream data
        if (_postTime and (flags & SOAPY_SDR_HAS_TIME) != 0)
        {
            _postTime = false;
            for (auto output : this->outputs())
            {
                output->postLabel("rxTime", timeNs, 0);
            }
        }
        if ((flags & SOAPY_SDR_END_BURST) != 0)
        {
            _postTime = true; //discontinuity: repost time on next receive
            for (auto output : this->outputs())
            {
                output->postLabel("rxEnd", true, ret-1);
            }
        }

        //discontinuity signaled but ok packet? post time on next call
        if ((flags & SOAPY_SDR_END_ABRUPT) != 0) _postTime = true;
    }

private:
    bool _postTime;
};

static Pothos::BlockRegistry registerSDRSource(
    "/soapy/source", &SDRSource::make);

static Pothos::BlockRegistry registerSDRSourceAlias(
    "/sdr/source", &SDRSource::make);
