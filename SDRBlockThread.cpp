// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SDRBlock.hpp"
#include <Poco/Format.h>
#include <Poco/Logger.h>
#include <Poco/SingletonHolder.h>
#include <cassert>
#include <iostream>

/*******************************************************************
 * threading configuration
 ******************************************************************/
void SDRBlock::setBackgroundMode(const std::string &mode)
{
    if (mode == "SETTERS_BLOCK")
    {
        _settersBlock = true;
        _activateBlocks = true;
    }
    else if (mode == "ACTIVATE_BLOCKS")
    {
        _settersBlock = false;
        _activateBlocks = true;
    }
    else if (mode == "ACTIVATE_THROWS")
    {
        _settersBlock = false;
        _activateBlocks = false;
    }
    else throw Pothos::InvalidArgumentException(
        "SDRBlock::setBackgroundMode("+mode+")", "unknown background mode");
}

void SDRBlock::enableEventSquash(const bool enable)
{
    _eventSquash = enable;
}

/*******************************************************************
 * Delayed method dispatch
 ******************************************************************/
Pothos::Object SDRBlock::opaqueCallHandler(const std::string &name, const Pothos::Object *inputArgs, const size_t numArgs)
{
    std::unique_lock<std::mutex> argsLock(_argsMutex);

    //check for existing errors, throw and clear
    if (_evalErrorValid)
    {
        std::rethrow_exception(_evalError);
        _evalErrorValid = false;
    }

    //put setters into the args cache
    const bool isSetter = (name.size() > 3 and name.substr(0, 3) == "set");
    if (isSetter)
    {
        bool addedToCache = false;
        for (auto &entry : _cachedArgs)
        {
            if (entry.first != name) continue;
            entry.second = Pothos::ObjectVector(inputArgs, inputArgs+numArgs);
            addedToCache = true;
            break;
        }
        if (not addedToCache) _cachedArgs.push_back(
            std::make_pair(name, Pothos::ObjectVector(inputArgs, inputArgs+numArgs)));
        _cond.notify_one();
        return Pothos::Object();
    }

    //block on cache to be emptied
    while (true)
    {
        if (not _cachedArgs.empty()) _cond.wait(argsLock);
        if (not _cachedArgs.empty()) continue;
        std::lock_guard<std::mutex> callLock(_callMutex);
        return Pothos::Block::opaqueCallHandler(name, inputArgs, numArgs);
    }

    //blocking call to the getter
    /*
    //try to setup the device future first
    if (name == "setupDevice") return Pothos::Block::opaqueCallHandler(name, inputArgs, numArgs);

    //when ready forward the call to the handler
    if (this->isReady()) return Pothos::Block::opaqueCallHandler(name, inputArgs, numArgs);

    //cache attempted settings when not ready
    const bool isSetter = (name.size() > 3 and name.substr(0, 3) == "set");
    if (isSetter) _cachedArgs.push_back(std::make_pair(name, Pothos::ObjectVector(inputArgs, inputArgs+numArgs)));
    else throw Pothos::Exception("SDRBlock::"+name+"()", "device not ready");
    return Pothos::Object();
    */
}

bool SDRBlock::isReady(void)
{
    std::unique_lock<std::mutex> argsLock(_argsMutex);

    //check for existing errors, throw and clear
    if (_evalErrorValid)
    {
        std::rethrow_exception(_evalError);
        _evalErrorValid = false;
    }

    //TODO remove this...
    while (true)
    {
        if (not _cachedArgs.empty()) _cond.wait(argsLock);
        if (not _cachedArgs.empty()) continue;
    }

    return _cachedArgs.empty();

    /*
    if (_device != nullptr) return true;
    if (_deviceFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
    _device = _deviceFuture.get();
    assert(_device != nullptr);

    //call the cached settings now that the device exists
    for (const auto &pair : _cachedArgs)
    {
        POTHOS_EXCEPTION_TRY
        {
            Pothos::Block::opaqueCallHandler(pair.first, pair.second.data(), pair.second.size());
        }
        POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
        {
            poco_error_f2(Poco::Logger::get("SDRBlock"), "call %s threw: %s", pair.first, ex.displayText());
        }
    }

    return true;
    */
}

/*******************************************************************
 * Evaluation thread
 ******************************************************************/
void SDRBlock::evalThreadLoop(void)
{
    while (not _evalThreadDone)
    {
        //wait for input settings args
        std::unique_lock<std::mutex> argsLock(_argsMutex);
        if (_cachedArgs.empty()) _cond.wait(argsLock);
        if (_cachedArgs.empty()) continue;

        //pop the most recent setting args
        std::pair<std::string, Pothos::ObjectVector> current;
        current = _cachedArgs.front();
        _cachedArgs.erase(_cachedArgs.begin());
        argsLock.unlock();

        //make the call in this thread
        std::lock_guard<std::mutex> callLock(_callMutex);
        POTHOS_EXCEPTION_TRY
        {
            Pothos::Block::opaqueCallHandler(current.first, current.second.data(), current.second.size());
        }
        POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
        {
            poco_error_f2(Poco::Logger::get("SDRBlock"), "call %s threw: %s", current.first, ex.displayText());
            _evalError = std::current_exception();
            _evalErrorValid = true;
        }
        _cond.notify_one();
    }
}
