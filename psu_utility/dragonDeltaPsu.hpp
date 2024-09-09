/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dragonChassisBasePsu.hpp"

/*
 * class DragonDeltaPsu: inherits DragonChassisBasePsu and implements
 * firmware update specific for Delta power supplies
 */
class DragonDeltaPsu : public DragonChassisBasePsu
{
  public:
    /*
     * Constructor:
     * inputs:
     * bus - i2c bus
     * address -i2c address
     * arbitrator - is there arbitrator present?
     * fwIdentifier - identifier expected for the firmware
     * imageName - path to image file
     */
    DragonDeltaPsu(int bus, int address, bool arbitrator,
                   const char* fwIdentifier, char* imageName);
    virtual ~DragonDeltaPsu();

  private:
    uint8_t fwIdent[15]; // hold fw identifier for the PSU

    virtual int unlock() override;
    virtual int erase() override;
    virtual int sendImage() override;
    virtual int activate() override;

    /*
     * checkStatus: reads the status register of the PSU
     * inputs:
     * read - pointer that will hold status register value
     * returns 0 for success error code for failure
     */
    int checkStatus(char* read);

    /*
     * sendCrc: send crc for the block of data to the PSU
     * inputs:
     * crc - crc to send
     * blockSize - the size of the block of data crc was calculated for
     * returns 0 for success error code for failure
     */
    int sendCrc(uint16_t crc, uint16_t blockSize);

    /*
     * readCrc: reads crc from PSU and compares with the expected value
     * inputs:
     * crc - expected crc
     * blockSize - the size of the block of data crc was calculated for
     * returns 0 for success error code for failure
     */
    int readCrc(uint16_t crc, uint16_t blockSize);
    void printFwVersion() override;
};
