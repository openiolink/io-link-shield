#pragma once
/*!
 * @file ProcessdateElements.h
 * @brief helper class that provides types for processdata conversion
 *
 * @copyright 2021, Balluff GmbH, all rights reserved
 * @author See AUTHORS file
 * @since 11.08.2022
 */

struct ProcessDataInfo_t
{
    double      gradient;
    double      offset;
    uint16_t    unitCode;
    std::string displayFormat;

    ProcessDataInfo_t()
        : gradient(1.0)
        , offset(0.0)
        , unitCode(0)
    {
    }
};
class ProcessDataElement
{
public:
	std::string key;
    ProcessDataInfo_t processDataInfo;
    std::string type;
    uint16_t bitOffset;
    uint16_t bitLength;
	uint16_t    subindex;

};