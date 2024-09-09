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

#include "dragonDeltaPsu.hpp"

#include <i2c/smbus.h>
#include <sys/stat.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#define CRC_MASK (1 << 6) | (1 << 7)

DragonChassisBasePsu::DragonChassisBasePsu(int bus, int address,
                                           bool arbitrator, char* imageName,
                                           const char* manufacturer,
                                           const char* model) :
    DragonChassisBase(bus, address, arbitrator, imageName),
    manufacturer(manufacturer), model(model)
{}

DragonChassisBasePsu::~DragonChassisBasePsu() {}

int DragonChassisBasePsu::checkManufacturer()
{
    int ret = 0;
    char read[17];     // maximum size of data
    char write = 0x99; // PMBUS manufacturer register

    ret = transfer(fd, 1, address, (uint8_t*)&write, (uint8_t*)read, 1, 14);
    if (ret)
        return static_cast<int>(UpdateError::ERROR_READ_MANUFACTURER);
    read[14] = '\0';
    std::string manufacturerBase(manufacturer);
    std::string manufacturerIn((const char*)read);
    boost::to_upper(manufacturerBase);
    boost::to_upper(manufacturerIn);
    if (manufacturerIn.find(manufacturerBase) == std::string::npos)
    {
        ret = static_cast<int>(UpdateError::ERROR_WRONG_MANUFACTURER);
    }

    write = 0x9A; // PMBUS model register
    ret = transfer(fd, 1, address, (uint8_t*)&write, (uint8_t*)read, 1, 16);
    if (ret)
        return static_cast<int>(UpdateError::ERROR_READ_MODEL);
    read[16] = '\0';
    std::string modelBase(model);
    std::string modelIn((const char*)read);
    boost::to_upper(modelBase);
    boost::to_upper(modelIn);
    if (modelIn.find(modelBase) == std::string::npos)
    {
        ret = static_cast<int>(UpdateError::ERROR_WRONG_MODEL);
    }

    return ret;
}

int DragonChassisBasePsu::fwUpdate()
{
    int ret = 0;
    int crcCnt = 0;

    ret = loadImage();
    if (ret)
        goto end;

    if (arb)
    {
        ret = enableArbitration(true);
        if (ret)
            goto end;
    }

    fd = openBus(bus);
    if (fd < 0)
    {
        ret = static_cast<int>(UpdateError::ERROR_OPEN_BUS);
        goto disable_arb;
    }

    // printFwVersion();

    ret = checkManufacturer();
    if (ret)
        goto close_fd;

    ret = unlock();
    if (ret)
    {
        goto close_fd;
    }

erase:
    ret = erase();
    if (ret)
    {
        goto close_fd;
    }

    ret = sendImage();
    if (ret)
    {
        if (ret != static_cast<int>(UpdateError::ERROR_READ_CRC) && crcCnt < 3)
        {
            goto close_fd;
        }
        crcCnt++;
        goto erase;
    }

    ret = activate();
    if (ret)
        goto close_fd;
close_fd:
    close(fd);

disable_arb:
    if (arb)
    {
        enableArbitration(false);
    }
end:
    return ret;
}
