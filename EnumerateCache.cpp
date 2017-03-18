// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Config.hpp>
#include <SoapySDR/Device.hpp>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

/*!
 * A singleton thread that polls for device enumeration.
 * The device enumeration is fetched in the background
 * so that the overlay operation does not block for devices.
 */
class SDRBlockBgEnumerator
{
public:
    SDRBlockBgEnumerator(void):
        _done(false),
        _cache(SoapySDR::Device::enumerate()), //force a synchronous read before backgrounding
        _bgThread(&SDRBlockBgEnumerator::pollingLoop, this)
    {
        return;
    }

    ~SDRBlockBgEnumerator(void)
    {
        _done = true;
        _cv.notify_one();
        _bgThread.join();
    }

    SoapySDR::KwargsList getCache(void)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _cache;
    }

private:

    void pollingLoop(void)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (not _done)
        {
            _cv.wait_for(lock, std::chrono::milliseconds(3000));
            if (_done) break;
            lock.unlock();
            auto result = SoapySDR::Device::enumerate();
            lock.lock();
            _cache = result;
        }
    }

    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _done;
    SoapySDR::KwargsList _cache;
    std::thread _bgThread;
};

SoapySDR::KwargsList cachedEnumerate(void)
{
    static SDRBlockBgEnumerator instance;
    return instance.getCache();
}
