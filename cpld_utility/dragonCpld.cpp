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

#include "dragonCpld.hpp"

#include "updateCPLDFw_dbus_log_event.h"

#include "utils.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

#define BUSY_BIT (1 << 12)
#define FAIL_BIT (1 << 13)
#define DONE_BIT (1 << 8)
#define CONF_BITS ((1 << 23) | (1 << 24) | (1 << 25))
#define PAGE_SIZE 16
#define REFRESH_GPIO_NAME "CPLD_REFRESH_IN_PRGRS_L-I"
#define I2C_RETRY_ATTEMPTS 5

extern const char* versionStr;

DragonCpld::DragonCpld(int devIdx, char* imageName, const char* config) :
    DragonChassisBase(devIdx, 0, true, imageName), config(config)

{}

DragonCpld::~DragonCpld()
{
    if (!arb && !rawLattice && cpldRefreshGpio)
        cpldRefreshGpio.reset();
}

int DragonCpld::fwUpdate()
{
    int ret = 0;
    std::string newFwVersion{};
    std::string objPath{};

    ret = loadConfig();
    if (ret)
        goto end;
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

    if (cpldRegBus != bus)
    {
        cpldRegFd = openBus(cpldRegBus);
        if (cpldRegFd < 0)
        {
            ret = static_cast<int>(UpdateError::ERROR_OPEN_BUS);
            goto close_fd;
        }
    }
    else
        cpldRegFd = fd;

    ret = readDeviceId();
    if (ret)
        goto close_cpld_fd;

    emitLogMessage(target_deter, devName.c_str(), versionStr, sev_info, NULL);

    ret = enableConfigurationInterface();
    if (ret)
        goto close_cpld_fd;

    ret = sendImage();
    if (ret)
        goto close_cpld_fd;

    emitLogMessage(update_succ, devName.c_str(), versionStr, sev_info, NULL);

    ret = activate();
    if (ret)
        goto close_cpld_fd;

    newFwVersion = readFwVersion();
    if (!newFwVersion.empty())
    {
        objPath = swBasePath + devName;
        setFwVersion(objPath, newFwVersion);
    }

close_cpld_fd:
    close(cpldRegFd);

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

int DragonCpld::readStatusRegister(uint32_t* value)
{
    const char write[] = "\x3c\x00\x00\x00"; // read status register command
                                             // from Lattice specification

    return transferWithRetry(fd, 1, address, (uint8_t*)write, (uint8_t*)value,
                             4, 4, I2C_RETRY_ATTEMPTS);
}

int DragonCpld::waitBusy(int wait, int errorCode)
{
    int cnt = 0;
    char cmd[] = "\xf0\x00\x00\x00"; // read busy register command from Lattice
                                     // specification
    char v;
    int ret = 0;

    while (cnt < wait)
    {
        ret = transferWithRetry(fd, 1, address, (uint8_t*)cmd, (uint8_t*)&v, 4,
                                1, I2C_RETRY_ATTEMPTS);
        if (ret)
            break;
        if (!(v & 0x80))
            break;
        sleep(1);
    }
    if (cnt >= wait)
    {
        ret = errorCode;
    }
    return ret;
}

int DragonCpld::readDeviceId()
{
    uint8_t cmd[] = {0xe0, 0x00, 0x00,
                     0x00}; // read device id command from Lattice specification
    uint8_t dev_id[5];
    int ret = 0;

    ret = transferWithRetry(fd, 1, address, (uint8_t*)cmd, dev_id, 4, 4,
                            I2C_RETRY_ATTEMPTS);
    if (ret)
        goto end;

    for (int i = 0; i < 4; i++)
        if (deviceIdArray[i] != dev_id[i])
            return static_cast<int>(UpdateError::ERROR_INVALID_DEVICE_ID);
end:
    return 0;
}

std::string DragonCpld::readFwVersion()
{
    // read device id command from Lattice specification
    uint8_t cmd[] = {0xc0, 0x00, 0x00};
    uint8_t fwVer[4];
    int ret = 0;
    int txLen = size(cmd) / sizeof(cmd[0]);
    int rxLen = size(fwVer) / sizeof(fwVer[0]);

    ret = transferWithRetry(fd, 1, address, (uint8_t*)cmd, fwVer, txLen, rxLen,
                            I2C_RETRY_ATTEMPTS);
    if (ret)
        return {};

    // Make it to string for updating FW version D-Bus property
    std::stringstream version;
    version << std::hex << std::setfill('0');
    bool validVersion = false;
    for (int i = 0; i < rxLen; i++)
    {
        if (fwVer[i])
        {
            validVersion = true;
        }

        version << "0x" << std::setw(2) << static_cast<int>(fwVer[i]);
        if (i < rxLen - 1)
        {
            version << " "; // Add space only between bytes
        }
    }

    if (validVersion)
    {
        return version.str();
    }
    else
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Invalid Firmware Version");
        return {};
    }
}

int DragonCpld::enableConfigurationInterface()
{
    // enable configuration interface command from Lattice specification
    uint8_t cmd[] = {0x74, 0x08, 0x00, 0x00};
    const int timeout = 5;
    int ret = 0;

    ret = sendDataWithRetry(
        fd, address, cmd, 4,
        static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER_WRITE),
        I2C_RETRY_ATTEMPTS);
    if (ret)
    {
        return ret;
    }

    ret = waitBusy(timeout,
                   static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER));
    if (ret)
    {
        return ret;
    }

    return 0;
}

int DragonCpld::erase(bool reset)
{
    // erase command from Lattice specification
    uint8_t cmd[] = {0x0E, 0x04, 0x00, 0x00};
    // reset command from Lattice specification
    uint8_t cmd_reset[] = {0x46, 0x00, 0x00, 0x00};
    const int timeout = 5;
    uint32_t reg;
    int ret;

    ret = sendDataWithRetry(fd, address, cmd, 4,
                            static_cast<int>(UpdateError::ERROR_ERASE),
                            I2C_RETRY_ATTEMPTS);
    if (ret)
        return ret;

    ret = waitBusy(timeout, static_cast<int>(UpdateError::ERROR_ERASE));
    if (ret)
    {
        return ret;
    }
    ret = readStatusRegister(&reg);
    if (ret)
        return static_cast<int>(UpdateError::ERROR_READ_STATUS_REG);

    if (reg & FAIL_BIT)
        return static_cast<int>(UpdateError::ERROR_ERASE);
    if (reset)
    {
        ret = sendDataWithRetry(fd, address, cmd_reset, 4,
                                static_cast<int>(UpdateError::ERROR_RESET),
                                I2C_RETRY_ATTEMPTS);
        if (ret)
            return ret;
    }

    return 0;
}

// Lattice CPLD is highly unstable chip when it comes to fw update
// the chip does not really have "cancel" capability. If verification
// of the image fails, the cleanup flow does not restore the old firmware
// try to do update one more time in hopes that it will work
#define VALIDATE_RETRY 1
int DragonCpld::sendImage()
{
    // send image command from Lattice specification
    // send command is 4 bytes, so it is sending 4 bytes
    // of the command and PAGE_SIZE of data
    uint8_t cmd[(PAGE_SIZE + 5)] = {0x70, 0x00, 0x00, 0x01};
    // done sending image command from Lattice specification
    uint8_t cmdDone[] = {0x5E, 0x00, 0x00, 0x00};
    uint8_t cmdSetPage[] = {0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int pageNum = 0;
    int remaining = imageSize;
    const int timeout = 5;
    int imageOffset = 0;
    uint32_t reg;
    int ret = 0;
    int toSend = PAGE_SIZE;
    bool repeat = false;

    numOfPagesWritten = 0;
    for (int k = 0; k < VALIDATE_RETRY; k++)
    {
        pageNum = 0;
        cmdSetPage[6] = 0;
        cmdSetPage[7] = 0;
        remaining = imageSize;
        imageOffset = 0;
        numOfPagesWritten = 0;

        ret = erase(true);
        if (ret)
            goto cleanup;

        while (remaining > 0)
        {
            repeat = false;
            toSend = std::min(remaining, PAGE_SIZE);
            memset(&cmd[4], 0, PAGE_SIZE);
            memcpy(&cmd[4], image + imageOffset, toSend);
            // we cannot use here sendDataWithRetry since the chip
            // will automatically increase the page number that is
            // being writen
            ret = sendData(fd, address, cmd, PAGE_SIZE + 4,
                           static_cast<int>(UpdateError::ERROR_SEND_IMAGE));
            // if i2c transaction has failed. we need to resend this page
            if (ret)
                repeat = true;

            ret = waitBusy(
                timeout, static_cast<int>(UpdateError::ERROR_SEND_IMAGE_BUSY));
            if (ret)
                goto cleanup;

            if (repeat)
            {
                // reset the page to the previous number
                ret = transferWithRetry(fd, 0, address, cmdSetPage, nullptr, 8,
                                        0, I2C_RETRY_ATTEMPTS);
                if (ret)
                    goto cleanup;
                continue;
            }
            pageNum++;
            pageNum &= 0xffff;
            cmdSetPage[6] = (pageNum >> 8) & 0xff;
            cmdSetPage[7] = pageNum & 0xff;
            remaining -= toSend;
            imageOffset += toSend;
            numOfPagesWritten++;
        }
        ret = validate();
        if (!ret)
            break;
        else
            emitLogMessage(ver_failed, devName.c_str(), versionStr, sev_crit,
                           NULL);
    }

    if (ret)
        goto cleanup;

    ret =
        sendDataWithRetry(fd, address, cmdDone, 4,
                          static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE),
                          I2C_RETRY_ATTEMPTS);
    if (ret)
        goto cleanup;
    ret = waitBusy(timeout,
                   static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_BUSY));
    if (ret)
    {
        return ret;
    }
    ret = readStatusRegister(&reg);
    if (ret)
        return static_cast<int>(UpdateError::ERROR_READ_STATUS_REG);

    if ((reg & BUSY_BIT) || (reg & FAIL_BIT))
    {
        cleanUp();
        return static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_CHECK);
    }

    return ret;

cleanup:
    cleanUp();
    return ret;
}

int DragonCpld::waitRefresh(int result)
{
    int ret = 0;
    int value;
    int cnt = 0;
    std::string refreshPath = "/sys/devices/platform/i2carb/cpld_refresh";
    const int arbTimeout = 15;
read_refresh:
    if (arb)
    {
        value = readSysFs(refreshPath);
        if (value < 0)
        {
            ret = static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH);
            goto end;
        }
    }
    else
    {
        value = cpldRefreshGpio.get_value();
    }

    if (value != result)
    {
        sleep(1);
        if (cnt++ < arbTimeout)
            goto read_refresh;
        if (result)
            ret =
                static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT);
        else
            ret = static_cast<int>(
                UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO);
        goto end;
    }
end:
    return ret;
}

int DragonCpld::validate()
{
    uint8_t cmdSetPage[] = {0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t readPages[] = {0x73, 0x00, 0x00, 0x01};
    uint8_t failDataChip[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                              0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    uint8_t failDataI2c[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                             0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    char readBuffer[PAGE_SIZE];
    int ret = 0;
    int imageOffset = 0;
    bool compareFailed = false;

    ret = transferWithRetry(fd, 0, address, cmdSetPage, nullptr, 8, 0,
                            I2C_RETRY_ATTEMPTS);
    if (ret)
    {
        return -1; // you need to add here an error for the case where we could
                   // not set the page
    }

    for (int i = 0; i < numOfPagesWritten; i++)
    {
        cmdSetPage[6] = (i >> 8) & 0xff;
        cmdSetPage[7] = i & 0xff;
        ret = transferWithRetry(fd, 1, address, readPages, (uint8_t*)readBuffer,
                                4, PAGE_SIZE, I2C_RETRY_ATTEMPTS);
        if (ret)
        {
            return -1; // add the case where we could not read the data back
        }

        if (memcmp(image + imageOffset, readBuffer, PAGE_SIZE))
        {
            // if there is additional traffic generated on the bus the cpld will
            // start returning fake data on the pages that are all 0's (usually
            // at the end of the cpld image)
            if (!memcmp(image + imageOffset, failDataChip, PAGE_SIZE) &&
                !memcmp(readBuffer, failDataI2c, PAGE_SIZE))
            {
                continue;
            }

            if (compareFailed)
            {
                ret = -1;
                goto end;
            }
            compareFailed = true;
            // reset i back to previous page
            i--;
            ret = transferWithRetry(fd, 0, address, cmdSetPage, nullptr, 8, 0,
                                    I2C_RETRY_ATTEMPTS);
            if (ret)
            {
                return -1; // you need to add here an error for the case where
                           // we could not set the page
            }
            continue;
        }
        compareFailed = false;
        imageOffset += PAGE_SIZE;
    }
end:
    return ret;
}

int DragonCpld::activate()
{
    // per cpld specification to activate an image the bmc must set 2 bits in
    // the cpld register in specific order. once this is done arbitrator will be
    // disabled on cpld side so raw access i2c must be used to talk to the cpld
    uint8_t cmd1[] = {0x10, 0x80};
    uint8_t cmd2[] = {0x10, 0x40};
    // refresh command from Lattice specification
    uint8_t cmd_refresh[] = {0x79, 0x40, 0x0};
    int ret = 0;
    int rawBusFd = 0;

    if (!rawLattice)
    {
        ret = sendData(cpldRegFd, cpldRegAddress, cmd1, 2,
                       static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG1));
        if (ret)
            goto end;
        ret = sendData(cpldRegFd, cpldRegAddress, cmd2, 2,
                       static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG2));
        if (ret)
            goto end;
        sleep(1);

        ret = waitRefresh(1);
        if (ret)
            goto end;
    }
    rawBusFd = openBus(cpldRawBus);
    if (rawBusFd < 0)
    {
        ret = static_cast<int>(UpdateError::ERROR_OPEN_BUS);
        goto end;
    }

    ret = transferWithRetry(rawBusFd, 0, address, cmd_refresh, nullptr, 3, 0,
                            I2C_RETRY_ATTEMPTS);
    if (ret)
    {
        ret = static_cast<int>(
            UpdateError::ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED);
        goto closeBus;
    }
    if (!rawLattice)
    {
        ret = waitRefresh(0);
        if (ret)
            goto closeBus;
    }
    else
        // Lattice status register is broken on these devices
        // we do not have reliable way to ping cpld to see if
        // cmd completed correctly. give it 30 seconds to come
        // back alive
        sleep(30);
closeBus:
    close(rawBusFd);
end:
    return ret;
}

int DragonCpld::cleanUp()
{
    int ret = 0;

    ret = erase(false);
    if (ret)
        return static_cast<int>(UpdateError::ERROR_CLEANUP_ERASE);

    ret = activate();
    return ret;
}

int DragonCpld::loadConfig()
{
    std::ifstream in(config);
    if (!in.good())
        return static_cast<int>(UpdateError::ERROR_CONFIG_NOT_FOUND);

    auto data = nlohmann::json::parse(in, nullptr, false);
    if (data.is_discarded())
        return static_cast<int>(UpdateError::ERROR_CONFIG_FORMAT);

    bool found = false;

    for (const auto& cpldInfo : data.at("CPLD"))
    {
        try
        {
            std::string arbS = cpldInfo.at("Arbitration");
            int arbN = stoi(arbS);
            if (arbN)
                arb = true;
            else
                arb = false;
            rawLattice = cpldInfo.at("RawLattice").get<bool>();

            // try find corresponding CPLD device in JSON
            int cpldDevNo = cpldInfo.at("CPLDDeviceNo");
            if (cpldDevNo == devIdx)
            {
                std::string devBus = cpldInfo.at("Bus");
                bus = stoi(devBus);
                std::string ad = cpldInfo.at("Address");
                address = handlHexString(ad);
                std::string cpldRb = cpldInfo.at("CpldRegBus");
                cpldRegBus = stoi(cpldRb);
                std::string cpldRa = cpldInfo.at("CpldRegAddress");
                cpldRegAddress = handlHexString(cpldRa);
                std::string cpldRaB = cpldInfo.at("CpldRawBus");
                cpldRawBus = stoi(cpldRaB);
                imageOffset = cpldInfo.at("ImageOffset");
                devName = cpldInfo.at("Name");
                found = true;
                for (size_t i = 0; i < deviceIdArray.size(); ++i)
                {
                    std::string hexValue = cpldInfo.at("DeviceId")[i];
                    deviceIdArray[i] =
                        static_cast<uint8_t>(std::stoi(hexValue, 0, 16));
                }
                break;
            }
        }
        catch (const std::exception& e)
        {
            continue;
        }
        if (!arb && !rawLattice)
        {
            cpldRefreshGpio = gpiod::find_line(REFRESH_GPIO_NAME);
            if (!cpldRefreshGpio)
                return static_cast<int>(UpdateError::ERROR_FAILED_TO_GET_GPIO);
            try
            {
                gpiod::line_request request{
                    "cpldUpdate", gpiod::line_request::DIRECTION_INPUT, 0};
                request.flags = gpiod::line_request::FLAG_ACTIVE_LOW;
                cpldRefreshGpio.request(request);
            }
            catch (const std::exception& e)
            {
                return static_cast<int>(UpdateError::ERROR_FAILED_TO_GET_GPIO);
            }
        }
    }

    if (!found)
        return static_cast<int>(UpdateError::ERROR_CONFIG_PARSE_FAILED);

    return 0;
}
