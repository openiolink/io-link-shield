#pragma once
/*!
 * @file IoddService.h
 * @brief IoddService class used for processdata conversion
 *
 * @copyright 2021, Balluff GmbH, all rights reserved
 * @author See AUTHORS file
 * @since 11.08.2022
 */
#include <memory>
#include <vector>
#include<list>
#include <nlohmann/json.hpp>
#include "ProcessdataElements.h"
#include <variant>
#include <chrono>
using IolDataReturn_t = std::variant<bool, uint64_t, int64_t, float, std::string, std::list<char>, long double, std::chrono::system_clock::time_point>;
class IoddService
{
 
public:
    IoddService() = default;
    ~IoddService() = default;

    IoddService(IoddService const&) = default;
    IoddService& operator=(IoddService const&) = default;

    IoddService(IoddService&&) noexcept = default;
    IoddService& operator=(IoddService&&) noexcept = default;

    std::tuple<nlohmann::json, nlohmann::json> interpretProcessData(std::vector<uint8_t> rawProcessData, uint16_t VendorID, uint32_t DeviceID, uint8_t RevisionID);

private:

    std::tuple<nlohmann::json, nlohmann::json> interpretProcessData(const std::vector <ProcessDataElement> iodd,const uint8_t* data, std::size_t dataLength, uint16_t conditionVariable);
    IolDataReturn_t getProcessDataVar(const ProcessDataElement& dataItem, const uint8_t* data, std::size_t dataLength);
    uint8_t getByteFromRight(const uint8_t* const pData, const std::size_t bitShiftRight);
 
    uint64_t getUInt64(const uint8_t* const pData, const uint16_t bitLength, const uint16_t bitOffset);
    void setBitLength(ProcessDataElement* element);

    std::unique_ptr<IoddService> mIoddService;
};

