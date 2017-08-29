// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyBlock.hpp"
#include <iostream>

/*******************************************************************
 * threading configuration
 ******************************************************************/
void SoapyBlock::setCallingMode(const std::string &mode)
{
    if (mode == "SYNCHRONOUS")
    {
        _backgrounding = false;
        _activateWaits = true;
    }
    else if (mode == "ACTIVATE_WAITS")
    {
        _backgrounding = true;
        _activateWaits = true;
    }
    else if (mode == "ACTIVATE_THROWS")
    {
        _backgrounding = true;
        _activateWaits = false;
    }
    else throw Pothos::InvalidArgumentException(
        "SoapyBlock::setBackgroundMode("+mode+")", "unknown background mode");
}

void SoapyBlock::setEventSquash(const bool enable)
{
    _eventSquash = enable;
}

/*******************************************************************
 * Delayed method dispatch
 ******************************************************************/
Pothos::Object SoapyBlock::opaqueCallHandler(const std::string &name, const Pothos::Object *inputArgs, const size_t numArgs)
{
    //Probes will call into the block again for the actual getter method.
    //To avoid a locking condition, call the probe here before the mutex.
    //This probe call itself does not touch the block internals.
    const bool isProbe = (name.size() > 5 and name.substr(0, 5) == "probe");
    if (isProbe) return Pothos::Block::opaqueCallHandler(name, inputArgs, numArgs);
    if (name == "overlay") return Pothos::Block::opaqueCallHandler(name, inputArgs, numArgs);

    std::unique_lock<std::mutex> argsLock(_argsMutex);

    //check for existing errors, throw and clear
    if (_evalErrorValid)
    {
        _evalErrorValid = false;
        std::rethrow_exception(_evalError);
    }

    //put setters into the args cache when backgrounding is enabled
    //or when squashing is enabled but only during block activation
    const bool isSetter = (name.size() > 3 and name.substr(0, 3) == "set");
    const bool background = _backgrounding or (_eventSquash and this->isActive());
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

bool SoapyBlock::isReady(void)
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
void SoapyBlock::evalThreadLoop(void)
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
            poco_error_f2(_logger, "call %s threw: %s", current.first, ex.displayText());
            argsLock.lock(); //re-lock to set exception
            _evalError = std::current_exception();
            _evalErrorValid = true;
            argsLock.unlock();
            _cond.notify_one();

            //setup device failed, this thread is done evaluation
            //the block will remain in a useless state until destructed
            if (current.first == "setupDevice") return;
        }
    }
}
