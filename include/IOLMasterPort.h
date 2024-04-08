/*!
 * @file IOLMasterPort.h
 * @brief Abstract class layer for driver ic independent port implementation. Defines methods 
 *        and variables for the IOLMasterPort<driver_ic> implementation.
 * @copyright 2022 Balluff GmbH
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *	    http://www.apache.org/licenses/LICENSE-2.0
 *
 *	 Unless required by applicable law or agreed to in writing, software
 *	 distributed under the License is distributed on an "AS IS" BASIS,
 *	 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	 See the License for the specific language governing permissions and
 *	 limitations under the License.
 * @author See AUTHORS file
 * @since 11.08.2022
 */
#ifndef IOLMASTERPORT_H_INCLUDED
#define IOLMASTERPORT_H_INCLUDED

//!***** Header-Files ***********************************************************
#include "Max14819.h"
#include <cstdint>

//!***** Implementation *********************************************************

class IOLMasterPort {
private:
    uint16_t portType_;
    uint16_t diModeSupport_;
    uint16_t portMode_;
    uint16_t portStatus_;
    uint16_t actualCycleTime_;
    uint16_t comSpeed_;
public:
    IOLMasterPort();
    virtual ~IOLMasterPort();
    virtual uint8_t begin() = 0;
    virtual uint8_t end() = 0;
    virtual void portHandler() = 0;
    virtual void readStatus() = 0;
    virtual void sendMCmd() = 0;
    virtual uint32_t readComSpeed() = 0;
    virtual void readPage() = 0;
    virtual void writePage() = 0;
    virtual uint8_t readISDU(vector<uint8_t> &data, uint16_t index, uint8_t subIndex) = 0;
    virtual uint8_t writeISDU(uint8_t sizeData, vector<uint8_t>& oData, uint16_t index, uint8_t subIndex) = 0;
	virtual uint8_t readDirectParameterPage(uint8_t address, uint8_t *pData) = 0;
    virtual uint8_t writePD(uint8_t sizeData, uint8_t *pData, uint8_t sizeAnswer) = 0;
    virtual void readDI() = 0;
    virtual void readCQ() = 0;
    virtual void writeCQ() = 0;
    virtual void isDeviceConnected() = 0;
};

#endif //IOLMASTERPORT_H_INCLUDED
