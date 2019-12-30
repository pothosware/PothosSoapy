// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <SoapySDR/ConverterRegistry.hpp>
#include <Poco/Format.h>
#include <algorithm>
#include <iostream>
#include <unordered_map>

using SoapySDR::ConverterRegistry;
using ConverterFunction = SoapySDR::ConverterRegistry::ConverterFunction;

static const std::unordered_map<std::string, std::string> PothosDTypeToSoapyFormat =
{
    {"int8",    SOAPY_SDR_S8},
    {"int16",   SOAPY_SDR_S16},
    {"int32",   SOAPY_SDR_S32},
    {"uint8",   SOAPY_SDR_U8},
    {"uint16",  SOAPY_SDR_U16},
    {"uint32",  SOAPY_SDR_U32},
    {"float32", SOAPY_SDR_F32},
    {"float64", SOAPY_SDR_F64},

    {"complex_int8",    SOAPY_SDR_CS8},
    {"complex_int16",   SOAPY_SDR_CS16},
    {"complex_int32",   SOAPY_SDR_CS32},
    {"complex_uint8",   SOAPY_SDR_CU8},
    {"complex_uint16",  SOAPY_SDR_CU16},
    {"complex_uint32",  SOAPY_SDR_CU32},
    {"complex_float32", SOAPY_SDR_CF32},
    {"complex_float64", SOAPY_SDR_CF64},
};

/***********************************************************************
 * |PothosDoc SoapySDR Converter
 *
 * Uses SoapySDR's converter infrastructure to convert all elements of the
 * input buffer to the given output type and output the result into the output
 * buffer.
 *
 * The performance of this block depends on the converters registered with
 * SoapySDR. This block will automatically use the fastest converter for the
 * given pair of types.
 *
 * |category /SDR
 *
 * |param inputDType[Input Data Type] The data type used by the input port.
 * |widget DTypeChooser(int=1,uint=1,float=1,cint=1,cuint=1,cfloat=1)
 * |default "float32"
 * |preview disable
 *
 * |param outputDType[Output Data Type] The data type used by the output port.
 * |widget DTypeChooser(int=1,uint=1,float=1,cint=1,cuint=1,cfloat=1)
 * |default "int16"
 * |preview disable
 *
 * |param scalar[Scalar] A factor multiplied to outputs when the types are sufficiently different sizes.
 * |widget DoubleSpinBox()
 * |default 1.0
 * |preview enable
 *
 * |factory /soapy/converter(inputDType,outputDType)
 * |setter setScalar(scalar)
 **********************************************************************/
class SoapyConverter : public Pothos::Block
{
public:
    static Pothos::Block* make(
        const Pothos::DType& inputDType,
        const Pothos::DType& outputDType)
    {
        return new SoapyConverter(inputDType, outputDType);
    }

    SoapyConverter(const Pothos::DType& inputDType, const Pothos::DType& outputDType):
        Pothos::Block(),
        _converterFunc(nullptr),
        _scalar(1.0)
    {
        std::string soapyInputFormat;
        std::string soapyOutputFormat;

        validateDTypeAndGetFormat(inputDType, &soapyInputFormat);
        validateDTypeAndGetFormat(outputDType, &soapyOutputFormat);

        assert(!soapyInputFormat.empty());
        assert(!soapyOutputFormat.empty());

        const auto availableTargetFormats = ConverterRegistry::listTargetFormats(soapyInputFormat);
        auto targetFormatIter = std::find(
                                    availableTargetFormats.begin(),
                                    availableTargetFormats.end(),
                                    soapyOutputFormat);
        if(availableTargetFormats.end() == targetFormatIter)
        {
            throw Pothos::InvalidArgumentException(
                      "No SoapySDR converter is registered for the given types",
                      Poco::format(
                          "%s -> %s",
                          inputDType.name(),
                          outputDType.name()));
        }

        _converterFunc = ConverterRegistry::getFunction(
                             soapyInputFormat,
                             soapyOutputFormat);
        assert(nullptr != _converterFunc);

        // With our types validated, set up the block.

        this->setupInput(0, inputDType);
        this->setupOutput(0, outputDType);

        this->registerCall(this, POTHOS_FCN_TUPLE(SoapyConverter, getScalar));
        this->registerCall(this, POTHOS_FCN_TUPLE(SoapyConverter, setScalar));
        this->registerProbe("getScalar", "scalarChanged", "setScalar");

        // Immediately trigger the signal.
        this->setScalar(_scalar);
    }

    double getScalar() const
    {
        return _scalar;
    };

    void setScalar(double scalar)
    {
        _scalar = scalar;

        this->emitSignal("scalarChanged", scalar);
    };

    void work() override
    {
        auto* inputPort = this->input(0);
        auto* outputPort = this->output(0);

        if(inputPort->elements() == 0) return;

        const size_t elems = std::min(
                                 inputPort->elements(),
                                 outputPort->elements());

        _converterFunc(
            inputPort->buffer().as<const void*>(),
            outputPort->buffer().as<void*>(),
            elems,
            _scalar);

        inputPort->consume(elems);
        outputPort->produce(elems);
    }

private:

    ConverterFunction _converterFunc;
    double _scalar;

    void validateDTypeAndGetFormat(
        const Pothos::DType& dtype,
        std::string* pSoapyFormat)
    {
        assert(nullptr != pSoapyFormat);

        auto inputFormatIter = PothosDTypeToSoapyFormat.find(dtype.name());
        if(PothosDTypeToSoapyFormat.end() != inputFormatIter)
        {
            (*pSoapyFormat) = inputFormatIter->second;
        }
        else
        {
            throw Pothos::InvalidArgumentException(
                      "The given DType does not have SoapySDR converter support",
                      dtype.name());
        }
    }
};

static Pothos::BlockRegistry registerSoapyConverter(
    "/soapy/converter", &SoapyConverter::make);
