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

DragonDeltaPsu::DragonDeltaPsu(int bus, int address, bool arbitrator,
                               const char* fwIdentifier, char* imageName) :
    DragonChassisBasePsu(bus, address, arbitrator, imageName, "DELTA",
                         "ECD56020022")
{
    std::memcpy(&fwIdent[2], fwIdentifier, 11);
    // unlock command sends 14 bytes total: Length (12), fwIdentifier (11
    // bytes), compatibility (1 byte), PEC
    fwIdent[0] = 0xf0; // this is erase command
    fwIdent[1] = 12;   // this is length
    fwIdent[13] = 0x1; // this is compatibility
}

DragonDeltaPsu::~DragonDeltaPsu() {}

int DragonDeltaPsu::unlock()
{
    char read[6] = {0};
    // to unlock PSU the fw identifier will be sent per PSU specification
    int ret = sendData(fd, address, fwIdent, 14,
                       static_cast<int>(UpdateError::ERROR_UNLOCK));
    if (ret)
        goto end;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    ret = checkStatus(read);
    if (ret)
        goto end;

    if (!(read[4] & 0x08))
        ret = static_cast<int>(UpdateError::ERROR_UNLOCK);

end:
    return ret;
}

int DragonDeltaPsu::erase()
{
    // erase command per PSU specification
    uint8_t cmd[4] = {0xf3, 0x01, 0x00};
    return sendData(fd, address, cmd, 3,
                    static_cast<int>(UpdateError::ERROR_ERASE));
}

int DragonDeltaPsu::sendCrc(uint16_t crc, uint16_t blockSize)
{
    uint8_t cmd[7];

    // send crc command per PSU specification
    cmd[0] = 0xf4;
    cmd[1] = 4;
    cmd[2] = blockSize & 0xff;
    cmd[3] = (blockSize >> 8) & 0xff;
    cmd[4] = crc & 0xff;
    cmd[5] = (crc >> 8) & 0xff;
    return sendData(fd, address, cmd, 6,
                    static_cast<int>(UpdateError::ERROR_SEND_CRC));
}

int DragonDeltaPsu::readCrc(uint16_t crc, uint16_t blockSize)
{
    char write = 0xf4;
    char read[6] = {0};
    int ret = 0;
    uint16_t checkCrc, checkBlockSize;

    ret = transfer(fd, 1, address, (uint8_t*)&write, (uint8_t*)read, 1, 6);
    if (ret)
        goto end;
    checkCrc = read[1] | (read[2] << 8);
    checkBlockSize = read[3] | (read[4] << 8);
    if (crc != checkCrc || blockSize != checkBlockSize)
    {
        ret = static_cast<int>(UpdateError::ERROR_READ_CRC);
    }
end:
    return ret;
}

int DragonDeltaPsu::sendImage()
{
    uint8_t cmd[69];
    uint16_t blockIndex = 0;
    int length = 64;
    int remaining = imageSize;
    int imageOffset = 0;
    int ret = 0;
    uint16_t crc = crc16((uint8_t*)image, imageSize);

    cmd[0] = 0xf2;
    while (remaining > 0)
    {
        char send = std::min(length, remaining);
        cmd[1] = send + 2;
        cmd[2] = blockIndex & 0xff;
        cmd[3] = (blockIndex >> 8) & 0xff;
        memcpy(&cmd[4], image + imageOffset, send);
        ret = sendData(fd, address, cmd, send + 4,
                       static_cast<int>(UpdateError::ERROR_SEND_IMAGE));
        if (ret)
            goto end;
        remaining -= send;
        imageOffset += send;
        blockIndex++;
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    ret = sendCrc(crc, blockIndex);
    if (ret)
        goto end;
    ret = readCrc(crc, blockIndex);
    if (ret)
        goto end;
end:
    return ret;
}

int DragonDeltaPsu::activate()
{
    // activate command per PSU specification
    uint8_t cmd[3] = {0x01, 0x00};
    char read[6] = {0};
    int ret = sendData(fd, address, cmd, 2,
                       static_cast<int>(UpdateError::ERROR_ACTIVATE));
    if (ret)
        goto end;
    sleep(40);
    ret = checkStatus(read);
    if (ret)
        goto end;

    if (ret || ((read[1]) & (CRC_MASK)) || ((read[2]) & (CRC_MASK)) ||
        ((read[4]) & (CRC_MASK)))
        ret = static_cast<int>(UpdateError::ERROR_ACTIVATE);

end:
    return ret;
}

int DragonDeltaPsu::checkStatus(char* read)
{
    // check status command per PSU specification
    char write = 0xf1;

    return transfer(fd, 1, address, (uint8_t*)&write, (uint8_t*)read, 1, 5);
}

void DragonDeltaPsu::printFwVersion()
{
    char write = 0x9B;
    char read[7] = {0};
    int ret = 0;

    ret = transfer(fd, 1, address, (uint8_t*)&write, (uint8_t*)read, 1, 6);
    if (ret)
        cout << "FAILED TO READ FW VERSION" << std::endl;
    read[6] = '\0';
    for (int i = 0; i < 6; i++)
        cout << " main version byte" << i << ":" << std::hex
             << static_cast<int>(read[i]) << std::endl;

    write = 0xE1;
    ret = transfer(fd, 1, address, (uint8_t*)&write, (uint8_t*)read, 1, 5);
    if (ret)
        cout << "FAILED TO READ FW VERSION" << std::endl;
    read[5] = '\0';
    for (int i = 0; i < 5; i++)
        cout << " bootloader version byte" << i << ":" << std::hex
             << static_cast<int>(read[i]) << std::endl;
    write = 0xE2;
    ret = transfer(fd, 1, address, (uint8_t*)&write, (uint8_t*)read, 1, 5);
    if (ret)
        cout << "FAILED TO READ FW VERSION" << std::endl;
    read[5] = '\0';
    for (int i = 0; i < 5; i++)
        cout << " application version byte" << i << ":" << std::hex
             << static_cast<int>(read[i]) << std::endl;
}
