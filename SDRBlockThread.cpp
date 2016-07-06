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
    if (mode == "SYNCHRONOUS")
    {
        _settersBlock = true;
        _activateWaits = true;
    }
    else if (mode == "ACTIVATE_WAITS")
    {
        _settersBlock = false;
        _activateWaits = true;
    }
    else if (mode == "ACTIVATE_THROWS")
    {
        _settersBlock = false;
        _activateWaits = false;
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
        _evalErrorValid = false;
        std::rethrow_exception(_evalError);
    }

    //put setters into the args cache when blocking is disabled
    //or when squashing is enabled during block activation
    const bool isSetter = (name.size() > 3 and name.substr(0, 3) == "set");
    const bool background = not _settersBlock or (_eventSquash and this->isActive());
    if (isSetter and background)
    {
        _cachedArgs.emplace_back(name, Pothos::ObjectVector(inputArgs, inputArgs+numArgs));
        argsLock.unlock();
        _cond.notify_one();
        return Pothos::Object();
    }

    //block on cached args to become empty
    while (not _cachedArgs.empty()) _cond.wait(argsLock);

    //make the blocking call in this context
    return Pothos::Block::opaqueCallHandler(name, inputArgs, numArgs);
}

bool SDRBlock::isReady(void)
{
    std::unique_lock<std::mutex> argsLock(_argsMutex);

    //check for existing errors, throw and clear
    if (_evalErrorValid)
    {
        _evalErrorValid = false;
        std::rethrow_exception(_evalError);
    }

    //when not blocking, we are ready when all cached args are processed
    if (not _activateWaits) return _cachedArgs.empty();

    //block on cached args to become empty
    while (not _cachedArgs.empty()) _cond.wait(argsLock);

    //all cached args processed, we are ready
    return true;
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
        auto current = _cachedArgs.front();
        _cachedArgs.erase(_cachedArgs.begin());

        //skip if there is a more recent setting
        bool skip = false;
        if (_eventSquash and this->isActive())
        {
            for (const auto &args : _cachedArgs)
            {
                if (current.first == args.first)
                {
                    skip = true;
                    break;
                }
            }
        }

        //done with cache, unlock to unblock main thread
        //and notify any blockers that may have been waiting
        argsLock.unlock();
        _cond.notify_one();
        if (skip) continue;

        //make the call in this thread
        POTHOS_EXCEPTION_TRY
        {
            Pothos::Block::opaqueCallHandler(current.first, current.second.data(), current.second.size());
        }
        POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
        {
            poco_error_f2(Poco::Logger::get("SDRBlock"), "call %s threw: %s", current.first, ex.displayText());
            argsLock.lock(); //re-lock to set exception
            _evalError = std::current_exception();
            _evalErrorValid = true;
        }
    }
}
