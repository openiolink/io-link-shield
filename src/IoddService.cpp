#include "IoddService.h"
/*!
 * @file IoddSetvice.cpp
 * @brief IoddService class used for processdata conversion
 *
 * @copyright 2021, Balluff GmbH, all rights reserved
 * @author See AUTHORS file
 * @since 11.08.2022
 */

void IoddService::setBitLength(ProcessDataElement *element)
{
    if (element->bitLength > 128 || element->bitLength == 0)
    {
        if (element->type == "BooleanT")
        {
            element->bitLength = 1;
        }
        if (element->type == "UIntegerT")
        {
            element->bitLength = 64;
        }
        if (element->type == "Float32T")
        {
            element->bitLength = 32;
        }
    }
}
/**
 * \brief Turns 8 bits into a byte.
 *
 * If the bits are shifted, this will read the remaining bits from the byte
 * before pData.
 *
 * Exactly 8 bits are extracted.  If less are required, the caller must
 * mask the return value.
 *
 * \param pData pointer to the byte containing the right-most (least significant) bit.
 * \param bitShiftRight number of bits needed to align the data
 * \return the extracted byte
 */
uint8_t IoddService::getByteFromRight(const uint8_t *const pData, const std::size_t bitShiftRight)
{
    uint8_t byte = 0;

    if (pData)
    {
        if (0 == bitShiftRight)
        {
            byte = *pData;
        }
        else
        {
            // Bits are not byte-aligned
            byte = ((static_cast<uint8_t>(*(pData - 1u)) << 8u) | *pData) >> bitShiftRight; // NOLINT(hicpp-signed-bitwise)
        }
    }

    return byte;
}
/**
 * \brief Converts a sequence of bits to an unsigned 64-bit integer.
 *
 * The sequence *MUST* start in the first byte!
 *
 * \warning
 * * There is no validation.
 * * Do not pass NULL as pData.
 * * Do not pass bitLength with 64 or higher.
 * * Do not pass bitOffset with 8 or higher.
 *
 * \param pData pointer to the byte which contains the most significant bit
 * of the integer to extract.
 * \param bitLength the number of bits to extract (1..64)
 * \param bitOffset the offset of the first bit from the left (0..7)
 */
uint64_t IoddService::getUInt64(const uint8_t *const pData, const uint16_t bitLength, const uint16_t bitOffset)
{
    uint64_t returnVal = 0;

    // Number of bits (0..7) that the source field is offset from the
    // right, relative to the next byte boundary.
    // The extracted bits will need to be shifted this many bits to the
    // right to align with the destination bytes.
    const std::size_t bitOffsetRight = (8u - ((static_cast<uint32_t>(bitOffset + bitLength)) & 7u)) & 7u;
    unsigned int bitsRemaining = bitLength;
    int bitsOld;
    do
    {
        returnVal |= (static_cast<uint64_t>(getByteFromRight(pData + ((bitOffset + bitsRemaining - 1u) >> 3u), bitOffsetRight)) & (bitsRemaining >= 8u ? 0xFFu : (0xFFu >> (8u - bitsRemaining))))
                     << (bitLength - bitsRemaining);

        bitsOld = static_cast<int>(bitsRemaining);
        bitsRemaining -= 8;

    } while (8 < bitsOld); // No more bits remaining

    return returnVal;
}
IolDataReturn_t IoddService::getProcessDataVar(const ProcessDataElement &dataItem, const uint8_t *data, std::size_t dataLength)
{
    const auto *const pProcessData = reinterpret_cast<const uint8_t *const>(data);
    uint16_t bitOffset = dataLength * 8 - dataItem.bitOffset - dataItem.bitLength;
    // Sanity check
    if (data == nullptr || dataItem.bitLength == 0)
    {
        return "Invalid";
    }

    if (dataItem.type == "BooleanT")
    {
        return static_cast<bool>(pProcessData[bitOffset >> 3u] & (1u << (7u - (bitOffset & 7u))));
    }
    if (dataItem.type == "UIntegerT")
    {
        if ((64 >= dataItem.bitLength) && (2 <= dataItem.bitLength))
        {
            return getUInt64(pProcessData + (bitOffset >> 3u), dataItem.bitLength, bitOffset & 7u);
        }
        return "Invalid";
    }
    if (dataItem.type == "Float32T")
    {
        // This type is always byte-aligned, even inside a RecordT.
        if (!(dataItem.bitOffset & 7u))
        {
            // Get pointer to first byte
            const uint8_t *const p = pProcessData + (bitOffset >> 3u);

            // Convert byte order and align to 4-byte boundary.
            // An uint32_t is used because it is the same size.
            const uint32_t tempIntVal = static_cast<uint32_t>((p[0] << 24u)) | static_cast<uint32_t>((p[1] << 16u)) | static_cast<uint32_t>((p[2] << 8u)) | static_cast<uint32_t>(p[3]);

            // Force the bytes from tempIntVal to be treated as a float.
            // This avoids an attempt to convert the value.  The next line
            // suppresses the cppcheck error message.
            // cppcheck-suppress invalidPointerCast
            return *reinterpret_cast<const float *>(&tempIntVal);
        }
        return "Invalid";
    }
    return "Invalid";
}

std::tuple<nlohmann::json, nlohmann::json> IoddService::interpretProcessData(const std::vector<ProcessDataElement> iodd, const uint8_t *data, std::size_t dataLength, uint16_t conditionVariable)
{

    // Initialize json
    nlohmann::json values(nlohmann::detail::value_t::object);
    nlohmann::json units(nlohmann::detail::value_t::object);

    for (ProcessDataElement element : iodd)
    {
        std::string key = element.key;
        IolDataReturn_t variable = getProcessDataVar(element, data, dataLength);
        ProcessDataInfo_t *processDataInfo = &element.processDataInfo;
        // Boolean
        if (std::holds_alternative<bool>(variable))
        {
            values[key] = std::get<bool>(variable);
            // Uint
        }
        else if (std::holds_alternative<uint64_t>(variable))
        {
            if (processDataInfo)
            {

                values[key] = processDataInfo->gradient * std::get<uint64_t>(variable) + processDataInfo->offset;
            }
            else
            {
                values[key] = std::get<uint64_t>(variable);
            }
            // Int
        }
        else if (std::holds_alternative<int64_t>(variable))
        {
            if (processDataInfo)
            {
                values[key] = processDataInfo->gradient * std::get<int64_t>(variable) + processDataInfo->offset;
            }
            else
            {
                values[key] = std::get<int64_t>(variable);
            }
            // Float
        }
        else if (std::holds_alternative<float>(variable))
        {
            if (processDataInfo)
            {

                values[key] = processDataInfo->gradient * std::get<float>(variable) + processDataInfo->offset;
            }
            else
            {
                values[key] = std::get<float>(variable);
            }
            // String
        }
        else if (std::holds_alternative<std::string>(variable))
        {
            values[key] = std::get<std::string>(variable);
            // OctetString
        }
        else if (std::holds_alternative<std::list<char>>(variable))
        {
            values[key] = std::get<std::list<char>>(variable);
            // TimeSpanT
        }
        else if (std::holds_alternative<long double>(variable))
        {
            values[key] = std::get<long double>(variable);
            // TimeT
        }
    }
    return std::make_tuple(values, units);
}

std::tuple<nlohmann::json, nlohmann::json> IoddService::interpretProcessData(std::vector<uint8_t> rawProcessData, uint16_t VendorID, uint32_t DeviceID, uint8_t RevisionID)
{
    uint16_t condition = 0;

    std::vector<ProcessDataElement> elements;

    std::vector<ProcessDataElement> SmartlightElements = {};
    ProcessDataElement TI_PD_Blinking_Segment1;
    ProcessDataElement TI_PD_Color_Segment1;
    ProcessDataElement TI_PD_Blinking_Segment2;
    ProcessDataElement TI_PD_Color_Segment2;
    ProcessDataElement TI_PD_Blinking_Segment3;
    ProcessDataElement TI_PD_Color_Segment3;
    ProcessDataElement TI_PD_SyncImp;
    ProcessDataElement TI_PD_SyncStart;
    TI_PD_Blinking_Segment1.key = "TI_PD_Blinking_Segment1";
    TI_PD_Blinking_Segment1.subindex = 1;
    TI_PD_Blinking_Segment1.type = "BooleanT";
    TI_PD_Blinking_Segment1.bitOffset = 11;
    TI_PD_Color_Segment1.key = "TI_PD_Color_Segment1";
    TI_PD_Color_Segment1.subindex = 2;
    TI_PD_Color_Segment1.type = "UIntegerT";
    TI_PD_Color_Segment1.bitLength = 3;
    TI_PD_Color_Segment1.bitOffset = 8;
    TI_PD_Blinking_Segment2.key = "TI_PD_Blinking_Segment2";
    TI_PD_Blinking_Segment2.subindex = 3;
    TI_PD_Blinking_Segment2.type = "BooleanT";
    TI_PD_Blinking_Segment2.bitOffset = 15;
    TI_PD_Color_Segment2.key = "TI_PD_Color_Segment2";
    TI_PD_Color_Segment2.subindex = 4;
    TI_PD_Color_Segment2.type = "UIntegerT";
    TI_PD_Color_Segment2.bitLength = 3;
    TI_PD_Color_Segment2.bitOffset = 12;
    TI_PD_Blinking_Segment3.key = "TI_PD_Blinking_Segment3";
    TI_PD_Blinking_Segment3.subindex = 5;
    TI_PD_Blinking_Segment3.type = "BooleanT";
    TI_PD_Blinking_Segment3.bitOffset = 3;
    TI_PD_Color_Segment3.key = "TI_PD_Color_Segment3";
    TI_PD_Color_Segment3.subindex = 6;
    TI_PD_Color_Segment3.type = "UIntegerT";
    TI_PD_Color_Segment3.bitLength = 3;
    TI_PD_Color_Segment3.bitOffset = 0;
    TI_PD_SyncImp.key = "TI_PD_SyncImp";
    TI_PD_SyncImp.subindex = 8;
    TI_PD_SyncImp.type = "BooleanT";
    TI_PD_SyncImp.bitOffset = 6;
    TI_PD_SyncStart.key = "TI_PD_SyncStart";
    TI_PD_SyncStart.subindex = 9;
    TI_PD_SyncStart.type = "BooleanT";
    TI_PD_SyncStart.bitOffset = 5;

    SmartlightElements.push_back(TI_PD_Blinking_Segment1);
    SmartlightElements.push_back(TI_PD_Color_Segment1);
    SmartlightElements.push_back(TI_PD_Blinking_Segment2);
    SmartlightElements.push_back(TI_PD_Color_Segment2);
    SmartlightElements.push_back(TI_PD_Blinking_Segment3);
    SmartlightElements.push_back(TI_PD_Color_Segment3);
    SmartlightElements.push_back(TI_PD_SyncImp);
    SmartlightElements.push_back(TI_PD_SyncStart);

    std::vector<ProcessDataElement> SmartlightElements_1 = {};

    ProcessDataElement TI_PD_Level;
    TI_PD_Level.key = "TI_PD_Level";
    TI_PD_Level.subindex = 1;
    TI_PD_Level.type = "UIntegerT";
    TI_PD_Level.bitLength = 3;
    TI_PD_Level.bitOffset = 2;

    SmartlightElements_1.push_back(TI_PD_Level);

    std::vector<ProcessDataElement> BawElements = {};

    ProcessDataElement TI_TargetPosition;
    TI_TargetPosition.key = "TI_TargetPosition";
    TI_TargetPosition.subindex = 1;
    TI_TargetPosition.type = "UIntegerT";
    TI_TargetPosition.bitLength = 3;
    TI_TargetPosition.bitOffset = 4;
    ProcessDataElement TI_OutOfRangeBit;
    TI_OutOfRangeBit.key = "TI_OutOfRangeBit";
    TI_OutOfRangeBit.subindex = 2;
    TI_OutOfRangeBit.type = "BooleanT";
    TI_OutOfRangeBit.bitOffset = 3;
    ProcessDataElement TI_BinaryChannel3;
    TI_BinaryChannel3.key = "TI_BinaryChannel3";
    TI_BinaryChannel3.subindex = 3;
    TI_BinaryChannel3.type = "BooleanT";
    TI_BinaryChannel3.bitOffset = 2;
    ProcessDataElement TI_BinaryChannel2;
    TI_BinaryChannel2.key = "TI_BinaryChannel2";
    TI_BinaryChannel2.subindex = 4;
    TI_BinaryChannel2.type = "BooleanT";
    TI_BinaryChannel2.bitOffset = 1;
    ProcessDataElement TI_BinaryChannel1;
    TI_BinaryChannel1.key = "TI_BinaryChannel1";
    TI_BinaryChannel1.subindex = 5;
    TI_BinaryChannel1.type = "BooleanT";
    TI_BinaryChannel1.bitOffset = 0;

    BawElements.push_back(TI_TargetPosition);
    BawElements.push_back(TI_OutOfRangeBit);
    BawElements.push_back(TI_BinaryChannel3);
    BawElements.push_back(TI_BinaryChannel2);
    BawElements.push_back(TI_BinaryChannel1);

    std::vector<ProcessDataElement> BesElements = {};

    ProcessDataElement TN_PDI_SSC1;
    TN_PDI_SSC1.key = "TN_PDI_SSC1";
    TN_PDI_SSC1.subindex = 1;
    TN_PDI_SSC1.type = "BooleanT";
    TN_PDI_SSC1.bitOffset = 0;
    ProcessDataElement TN_PDI_OUT_OF_RANGE;
    TN_PDI_OUT_OF_RANGE.key = "TN_PDI_OUT_OF_RANGE";
    TN_PDI_OUT_OF_RANGE.subindex = 2;
    TN_PDI_OUT_OF_RANGE.type = "BooleanT";
    TN_PDI_OUT_OF_RANGE.bitOffset = 1;
    ProcessDataElement TN_PDI_SPEED_TOO_LOW;
    TN_PDI_SPEED_TOO_LOW.key = "TN_PDI_SPEED_TOO_LOW";
    TN_PDI_SPEED_TOO_LOW.subindex = 3;
    TN_PDI_SPEED_TOO_LOW.type = "BooleanT";
    TN_PDI_SPEED_TOO_LOW.bitOffset = 2;
    ProcessDataElement TN_PDI_SPEED_TOO_HIGH;
    TN_PDI_SPEED_TOO_HIGH.key = "TN_PDI_SPEED_TOO_HIGH";
    TN_PDI_SPEED_TOO_HIGH.subindex = 4;
    TN_PDI_SPEED_TOO_HIGH.type = "BooleanT";
    TN_PDI_SPEED_TOO_HIGH.bitOffset = 3;
    ProcessDataElement TN_PDI_TEACH_ACTIVE;
    TN_PDI_TEACH_ACTIVE.key = "TN_PDI_TEACH_ACTIVE";
    TN_PDI_TEACH_ACTIVE.subindex = 5;
    TN_PDI_TEACH_ACTIVE.type = "BooleanT";
    TN_PDI_TEACH_ACTIVE.bitOffset = 4;
    ProcessDataElement TN_PDI_TEACH_SUCCESS;
    TN_PDI_TEACH_SUCCESS.key = "TN_PDI_TEACH_SUCCESS";
    TN_PDI_TEACH_SUCCESS.subindex = 6;
    TN_PDI_TEACH_SUCCESS.type = "BooleanT";
    TN_PDI_TEACH_SUCCESS.bitOffset = 5;
    ProcessDataElement TN_PDI_TEACH_ERROR;
    TN_PDI_TEACH_ERROR.key = "TN_PDI_TEACH_ERROR";
    TN_PDI_TEACH_ERROR.subindex = 7;
    TN_PDI_TEACH_ERROR.type = "BooleanT";
    TN_PDI_TEACH_ERROR.bitOffset = 6;
    ProcessDataElement TN_PDI_COUNT_LIMIT;
    TN_PDI_COUNT_LIMIT.key = "TN_PDI_COUNT_LIMIT";
    TN_PDI_COUNT_LIMIT.subindex = 8;
    TN_PDI_COUNT_LIMIT.type = "BooleanT";
    TN_PDI_COUNT_LIMIT.bitOffset = 7;
    ProcessDataElement TN_PDI_COUNT;
    TN_PDI_COUNT.key = "TN_PDI_COUNT";
    TN_PDI_COUNT.subindex = 9;
    TN_PDI_COUNT.type = "UIntegerT";
    TN_PDI_COUNT.bitOffset = 8;
    TN_PDI_COUNT.bitLength = 16;

    BesElements.push_back(TN_PDI_SSC1);
    BesElements.push_back(TN_PDI_OUT_OF_RANGE);
    BesElements.push_back(TN_PDI_SPEED_TOO_LOW);
    BesElements.push_back(TN_PDI_SPEED_TOO_HIGH);
    BesElements.push_back(TN_PDI_TEACH_ACTIVE);
    BesElements.push_back(TN_PDI_TEACH_SUCCESS);
    BesElements.push_back(TN_PDI_TEACH_ERROR);
    BesElements.push_back(TN_PDI_COUNT_LIMIT);
    BesElements.push_back(TN_PDI_COUNT);

    std::vector<ProcessDataElement> BcmElements = {};

    ProcessDataElement TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_X;
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_X.key = "TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_X";
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_X.subindex = 1;
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_X.type = "Float32T";
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_X.bitOffset = 128;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_X);
    ProcessDataElement TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Y;
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Y.key = "TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Y";
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Y.subindex = 2;
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Y.type = "Float32T";
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Y.bitOffset = 96;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Y);
    ProcessDataElement TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Z;
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Z.key = "TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Z";
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Z.subindex = 3;
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Z.type = "Float32T";
    TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Z.bitOffset = 64;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_Vibration_Veloc_RMS_v_RMS_Z);
    ProcessDataElement TI_PD_In_Vibration_Veloc_Contact_Temp_Contact_Temp;
    TI_PD_In_Vibration_Veloc_Contact_Temp_Contact_Temp.key = "TI_PD_In_Vibration_Veloc_Contact_Temp_Contact_Temp";
    TI_PD_In_Vibration_Veloc_Contact_Temp_Contact_Temp.subindex = 4;
    TI_PD_In_Vibration_Veloc_Contact_Temp_Contact_Temp.type = "Float32T";
    TI_PD_In_Vibration_Veloc_Contact_Temp_Contact_Temp.bitOffset = 32;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_Contact_Temp_Contact_Temp);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_X_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_X_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_X_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_X_Status.subindex = 5;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_X_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_X_Status.bitOffset = 31;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_X_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_X_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_X_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_X_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_X_Status.subindex = 6;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_X_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_X_Status.bitOffset = 30;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_X_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Y_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Y_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Y_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Y_Status.subindex = 7;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Y_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Y_Status.bitOffset = 29;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Y_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Y_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Y_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Y_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Y_Status.subindex = 8;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Y_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Y_Status.bitOffset = 28;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Y_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Z_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Z_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Z_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Z_Status.subindex = 9;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Z_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Z_Status.bitOffset = 27;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_Z_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Z_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Z_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Z_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Z_Status.subindex = 10;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Z_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Z_Status.bitOffset = 26;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_Z_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_M_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_M_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_M_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_M_Status.subindex = 11;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_M_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_M_Status.bitOffset = 25;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_a_RMS_M_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_M_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_M_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_M_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_M_Status.subindex = 12;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_M_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_M_Status.bitOffset = 24;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_a_RMS_M_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_X_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_X_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_X_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_X_Status.subindex = 13;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_X_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_X_Status.bitOffset = 23;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_X_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_X_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_X_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_X_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_X_Status.subindex = 14;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_X_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_X_Status.bitOffset = 22;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_X_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Y_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Y_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Y_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Y_Status.subindex = 15;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Y_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Y_Status.bitOffset = 21;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Y_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Y_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Y_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Y_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Y_Status.subindex = 16;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Y_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Y_Status.bitOffset = 20;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Y_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Z_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Z_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Z_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Z_Status.subindex = 17;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Z_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Z_Status.bitOffset = 19;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_Z_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Z_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Z_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Z_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Z_Status.subindex = 18;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Z_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Z_Status.bitOffset = 18;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_Z_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_M_Status;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_M_Status.key = "TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_M_Status";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_M_Status.subindex = 19;
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_M_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_M_Status.bitOffset = 17;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_PreAlarm_v_RMS_M_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_M_Status;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_M_Status.key = "TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_M_Status";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_M_Status.subindex = 20;
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_M_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_M_Status.bitOffset = 16;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_MainAlarm_v_RMS_M_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Reserved;
    TI_PD_In_Vibration_Veloc_SB_Reserved.key = "TI_PD_In_Vibration_Veloc_SB_Reserved";
    TI_PD_In_Vibration_Veloc_SB_Reserved.subindex = 21;
    TI_PD_In_Vibration_Veloc_SB_Reserved.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Reserved.bitOffset = 15;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Reserved);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_A;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_A.key = "TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_A";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_A.subindex = 22;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_A.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_A.bitOffset = 14;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_A);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_B;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_B.key = "TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_B";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_B.subindex = 23;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_B.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_B.bitOffset = 13;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_B);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_C;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_C.key = "TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_C";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_C.subindex = 24;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_C.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_C.bitOffset = 12;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_C);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_D;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_D.key = "TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_D";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_D.subindex = 25;
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_D.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_D.bitOffset = 11;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Vibration_Severity_Zone_D);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Reserved1;
    TI_PD_In_Vibration_Veloc_SB_Reserved1.key = "TI_PD_In_Vibration_Veloc_SB_Reserved1";
    TI_PD_In_Vibration_Veloc_SB_Reserved1.subindex = 26;
    TI_PD_In_Vibration_Veloc_SB_Reserved1.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Reserved1.bitOffset = 10;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Reserved1);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Reserved2;
    TI_PD_In_Vibration_Veloc_SB_Reserved2.key = "TI_PD_In_Vibration_Veloc_SB_Reserved2";
    TI_PD_In_Vibration_Veloc_SB_Reserved2.subindex = 27;
    TI_PD_In_Vibration_Veloc_SB_Reserved2.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Reserved2.bitOffset = 9;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Reserved2);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Reserved3;
    TI_PD_In_Vibration_Veloc_SB_Reserved3.key = "TI_PD_In_Vibration_Veloc_SB_Reserved3";
    TI_PD_In_Vibration_Veloc_SB_Reserved3.subindex = 28;
    TI_PD_In_Vibration_Veloc_SB_Reserved3.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Reserved3.bitOffset = 8;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Reserved3);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Lower_Alarm_Status;
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Lower_Alarm_Status.key = "TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Lower_Alarm_Status";
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Lower_Alarm_Status.subindex = 29;
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Lower_Alarm_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Lower_Alarm_Status.bitOffset = 7;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Lower_Alarm_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Upper_Alarm_Status;
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Upper_Alarm_Status.key = "TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Upper_Alarm_Status";
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Upper_Alarm_Status.subindex = 30;
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Upper_Alarm_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Upper_Alarm_Status.bitOffset = 6;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Contact_Temp_Upper_Alarm_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Reserved4;
    TI_PD_In_Vibration_Veloc_SB_Reserved4.key = "TI_PD_In_Vibration_Veloc_SB_Reserved4";
    TI_PD_In_Vibration_Veloc_SB_Reserved4.subindex = 31;
    TI_PD_In_Vibration_Veloc_SB_Reserved4.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Reserved4.bitOffset = 5;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Reserved4);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Reserved5;
    TI_PD_In_Vibration_Veloc_SB_Reserved5.key = "TI_PD_In_Vibration_Veloc_SB_Reserved5";
    TI_PD_In_Vibration_Veloc_SB_Reserved5.subindex = 32;
    TI_PD_In_Vibration_Veloc_SB_Reserved5.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Reserved5.bitOffset = 4;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Reserved5);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_AmbPressure_Lower_Alarm_Status;
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Lower_Alarm_Status.key = "TI_PD_In_Vibration_Veloc_SB_AmbPressure_Lower_Alarm_Status";
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Lower_Alarm_Status.subindex = 33;
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Lower_Alarm_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Lower_Alarm_Status.bitOffset = 3;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_AmbPressure_Lower_Alarm_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_AmbPressure_Upper_Alarm_Status;
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Upper_Alarm_Status.key = "TI_PD_In_Vibration_Veloc_SB_AmbPressure_Upper_Alarm_Status";
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Upper_Alarm_Status.subindex = 35;
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Upper_Alarm_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_AmbPressure_Upper_Alarm_Status.bitOffset = 2;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_AmbPressure_Upper_Alarm_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Humidty_Lower_Alarm_Status;
    TI_PD_In_Vibration_Veloc_SB_Humidty_Lower_Alarm_Status.key = "TI_PD_In_Vibration_Veloc_SB_Humidty_Lower_Alarm_Status";
    TI_PD_In_Vibration_Veloc_SB_Humidty_Lower_Alarm_Status.subindex = 37;
    TI_PD_In_Vibration_Veloc_SB_Humidty_Lower_Alarm_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Humidty_Lower_Alarm_Status.bitOffset = 1;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Humidty_Lower_Alarm_Status);
    ProcessDataElement TI_PD_In_Vibration_Veloc_SB_Humidity_Upper_Alarm_Status;
    TI_PD_In_Vibration_Veloc_SB_Humidity_Upper_Alarm_Status.key = "TI_PD_In_Vibration_Veloc_SB_Humidity_Upper_Alarm_Status";
    TI_PD_In_Vibration_Veloc_SB_Humidity_Upper_Alarm_Status.subindex = 39;
    TI_PD_In_Vibration_Veloc_SB_Humidity_Upper_Alarm_Status.type = "BooleanT";
    TI_PD_In_Vibration_Veloc_SB_Humidity_Upper_Alarm_Status.bitOffset = 0;
    BcmElements.push_back(TI_PD_In_Vibration_Veloc_SB_Humidity_Upper_Alarm_Status);

    if (DeviceID == 330242)
    {
        for (size_t i = 0; i < SmartlightElements_1.size(); i++)
        {
            setBitLength(&SmartlightElements_1[i]);
            elements.push_back(SmartlightElements_1[i]);
        }
    }

    if (DeviceID == 917762)
    {
        for (size_t i = 0; i < BcmElements.size(); i++)
        {
            setBitLength(&BcmElements[BcmElements.size() - 1 - i]);
            elements.push_back(BcmElements[BcmElements.size() - 1 - i]);
        }
    }

    if (DeviceID == 131330)
    {
        for (size_t i = 0; i < BawElements.size(); i++)
        {
            setBitLength(&BawElements[i]);
            elements.push_back(BawElements[i]);
        }
    }

    if (DeviceID == 132099)
    {
        for (size_t i = 0; i < BesElements.size(); i++)
        {
            setBitLength(&BesElements[i]);
            elements.push_back(BesElements[i]);
        }
    }

    try
    {
        const std::tuple<nlohmann::json, nlohmann::json> transformedData = interpretProcessData(elements, rawProcessData.data(), rawProcessData.size(), condition);
        return transformedData;
    }
    catch (int e)
    {
        const std::tuple<nlohmann::json, nlohmann::json> emptyJson;
        return emptyJson;
    }
}