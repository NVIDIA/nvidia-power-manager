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

#include "i2c.hpp"

#include <stdint.h>
#include <sys/stat.h>

#include <phosphor-logging/log.hpp>

#include <fstream>
#include <string>

#define ARB_STATE_PATH "/sys/devices/platform/i2carb/arb_state"

/*
 * DragonChassisBase class is virtual class that will be used for Dragon PSU and
 * CPLD update. class will hold info about general purpose variables and general
 * purpose methods.
 */

class DragonChassisBase : public I2c
{
  public:
    /*Constructor:
     * inputs:
     * bus - i2c bus number to use
     * address - i2c address to use
     * arbitrator - is bus arbitrated or not
     * imageName - path to the file holding update data
     */
    DragonChassisBase(int bus, int address, bool arbitrator, char* imageName) :
        bus(bus), address(address), arb(arbitrator), imageName(imageName),
        image(nullptr){};

    virtual ~DragonChassisBase()
    {
        if (image)
            free(image);
    }

    /* printError:
     * The method converts enum to the textual representation of the error code
     * inputs:
     * code - enum for the error code
     */
    void printError(int code)
    {
        using namespace phosphor::logging;

        std::string msg;
        switch (code)
        {
            case static_cast<int>(UpdateError::ERROR_LOAD_IMAGE):
                msg = "UpdateError::ERROR_LOAD_IMAGE";
                break;
            case static_cast<int>(UpdateError::ERROR_ENABLE_ARB):
                msg = "UpdateError::ERROR_ENABLE_ARB";
                break;
            case static_cast<int>(UpdateError::ERROR_DISABLE_ARB):
                msg = "UpdateError::ERROR_DISABLE_ARB";
                break;
            case static_cast<int>(UpdateError::ERROR_OPEN_BUS):
                msg = "UpdateError::ERROR_OPEN_BUS";
                break;
            case static_cast<int>(UpdateError::ERROR_UNLOCK):
                msg = "UpdateError::ERROR_UNLOCK";
                break;
            case static_cast<int>(UpdateError::ERROR_ERASE):
                msg = "UpdateError::ERROR_LOAD_IMAGE";
                break;
            case static_cast<int>(UpdateError::ERROR_SEND_IMAGE):
                msg = "UpdateError::ERROR_SEND_IMAGE";
                break;
            case static_cast<int>(UpdateError::ERROR_SEND_CRC):
                msg = "UpdateError::ERROR_SEND_CRC";
                break;
            case static_cast<int>(UpdateError::ERROR_READ_CRC):
                msg = "UpdateError::ERROR_READ_CRC";
                break;
            case static_cast<int>(UpdateError::ERROR_ACTIVATE):
                msg = "UpdateError::ERROR_ACTIVATE";
                break;
            case static_cast<int>(UpdateError::ERROR_OPEN_IMAGE):
                msg = "UpdateError::ERROR_OPEN_IMAGE";
                break;
            case static_cast<int>(UpdateError::ERROR_STAT_IMAGE):
                msg = "UpdateError::ERROR_STAT_IMAGE";
                break;
            case static_cast<int>(UpdateError::ERROR_WRONG_MANUFACTURER):
                msg = "UpdateError::ERROR_WRONG_MANUFACTURER";
                break;
            case static_cast<int>(UpdateError::ERROR_READ_MANUFACTURER):
                msg = "UpdateError::ERROR_READ_MANUFACTURER";
                break;
            case static_cast<int>(UpdateError::ERROR_READ_STATUS_REG):
                msg = "UpdateError::ERROR_READ_STATUS_REG";
                break;
            case static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER):
                msg = "UpdateError::ERROR_ENABLE_CFG_INTERFACE";
                break;
            case static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER_WRITE):
                msg = "UpdateError::ERROR_ENABLE_CFG_INTER_WRITE";
                break;
            case static_cast<int>(UpdateError::ERROR_RESET):
                msg = "UpdateError::ERROR_RESET";
                break;
            case static_cast<int>(UpdateError::ERROR_ARB_NOT_FOUND):
                msg = "UpdateError::ERROR_ARB_NOT_FOUND";
                break;
            case static_cast<int>(UpdateError::ERROR_WRONG_MODEL):
                msg = "UpdateError::ERROR_WRONG_MODEL";
                break;
            case static_cast<int>(UpdateError::ERROR_READ_MODEL):
                msg = "UpdateError::ERROR_READ_MODEL";
                break;
            case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_BUSY):
                msg = "UpdateError::ERROR_SEND_IMAGE_BUSY";
                break;
            case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE):
                msg = "UpdateError::ERROR_SEND_IMAGE_DONE";
                break;
            case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_BUSY):
                msg = "UpdateError::ERROR_SEND_IMAGE_DONE_BUSY";
                break;
            case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_CHECK):
                msg = "UpdateError::ERROR_SEND_IMAGE_DONE_CHECK";
                break;
            case static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG1):
                msg = "UpdateError::ERROR_ACTIVATE_CPLD_REG1";
                break;
            case static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG2):
                msg = "UpdateError::ERROR_ACTIVATE_CPLD_REG2";
                break;
            case static_cast<int>(UpdateError::ERROR_CPLD_REFRESH_NOT_FOUND):
                msg = "UpdateError::ERROR_CPLD_REFRESH_NOT_FOUND";
                break;
            case static_cast<int>(UpdateError::ERROR_CPLD_REFRESH_FAILED):
                msg = "UpdateError::ERROR_CPLD_REFRESH_FAILED";
                break;
            case static_cast<int>(
                UpdateError::ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED):
                msg = "UpdateError::ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED";
                break;
            case static_cast<int>(
                UpdateError::ERROR_INTERNAL_CPLD_REFRESH_FAILED):
                msg = "UpdateError::ERROR_INTERNAL_CPLD_REFRESH_FAILED";
                break;
            case static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH):
                msg = "UpdateError::ERROR_READ_CPLD_REFRESH";
                break;
            case static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT):
                msg = "UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT";
                break;
            case static_cast<int>(
                UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO):
                msg = "UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO";
                break;
            case static_cast<int>(UpdateError::ERROR_CLEANUP_ERASE):
                msg = "UpdateError::ERROR_CLEANUP_ERASE";
                break;
            case static_cast<int>(UpdateError::ERROR_CONFIG_NOT_FOUND):
                msg = "UpdateError::ERROR_CONFIG_NOT_FOUND";
                break;
            case static_cast<int>(UpdateError::ERROR_CONFIG_FORMAT):
                msg = "UpdateError::ERROR_CONFIG_FORMAT";
                break;
            case static_cast<int>(UpdateError::ERROR_CONFIG_PARSE_FAILED):
                msg = "UpdateError::ERROR_CONFIG_PARSE_FAILED";
                break;
            case static_cast<int>(UpdateError::ERROR_INVALID_DEVICE_ID):
                msg = "UpdateError::ERROR_INVALID_DEVICE_ID";
                break;
            case static_cast<int>(UpdateError::ERROR_FAILED_TO_GET_GPIO):
                msg = "UpdateError::ERROR_FAILED_TO_GET_GPIO";
                break;
            case 0:
                msg = "Update was successful";
                break;
            default:
                msg = "UpdateError::Invalid Error code";
                break;
        }
        std::cerr << msg.c_str() << std::endl;
        log<level::ERR>(msg.c_str());
    }

    /*
     * enum list for all possible error codes
     */
    enum class UpdateError
    {
        ERROR_LOAD_IMAGE = 1,
        ERROR_ENABLE_ARB = 2,
        ERROR_DISABLE_ARB = 3,
        ERROR_OPEN_BUS = 4,
        ERROR_UNLOCK = 5,
        ERROR_ERASE = 6,
        ERROR_SEND_IMAGE = 7,
        ERROR_SEND_CRC = 8,
        ERROR_READ_CRC = 9,
        ERROR_ACTIVATE = 10,
        ERROR_OPEN_IMAGE = 11,
        ERROR_STAT_IMAGE = 12,
        ERROR_WRONG_MANUFACTURER = 13,
        ERROR_READ_MANUFACTURER = 14,
        ERROR_READ_STATUS_REG = 15,
        ERROR_ENABLE_CFG_INTER = 16,
        ERROR_ENABLE_CFG_INTER_WRITE = 17,
        ERROR_RESET = 18,
        ERROR_ARB_NOT_FOUND = 19,
        ERROR_WRONG_MODEL = 20,
        ERROR_READ_MODEL = 21,
        ERROR_SEND_IMAGE_BUSY = 22,
        ERROR_SEND_IMAGE_DONE = 23,
        ERROR_SEND_IMAGE_DONE_BUSY = 24,
        ERROR_SEND_IMAGE_DONE_CHECK = 25,
        ERROR_ACTIVATE_CPLD_REG1 = 26,
        ERROR_ACTIVATE_CPLD_REG2 = 27,
        ERROR_CPLD_REFRESH_NOT_FOUND = 28,
        ERROR_CPLD_REFRESH_FAILED = 29,
        ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED = 30,
        ERROR_INTERNAL_CPLD_REFRESH_FAILED = 31,
        ERROR_READ_CPLD_REFRESH = 32,
        ERROR_READ_CPLD_REFRESH_TIMEOUT = 33,
        ERROR_CLEANUP_ERASE = 34,
        ERROR_CONFIG_NOT_FOUND = 35,
        ERROR_CONFIG_FORMAT = 36,
        ERROR_CONFIG_PARSE_FAILED = 37,
        ERROR_INVALID_DEVICE_ID = 38,
        ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO = 39,
        ERROR_FAILED_TO_GET_GPIO = 40,
    };

  protected:
    int bus;                 // i2 bus number
    int address;             // i2c address
    bool arb;                // is arbitrator needed?
    bool rawLattice;         // are we directly talking to Lattice part?
    char* imageName;         // path to image file
    char* image;             // pointer to the image read in the memory
    int imageSize;           // size of the image in the memory
    int fd;                  // image file descriptor
    std::array<uint8_t, 4> deviceIdArray;
    const int arbTrials = 5; // how many times to try to change arb state

    /*
     * readSysFs: the method will read and return a number from sysfs file
     * inputs:
     * path - path to the file to read from
     */
    int readSysFs(std::string path)
    {
        int ret = 0;
        std::string s;
        std::ifstream f(path.c_str());
        if (!f)
            return -1;
        f >> s;
        f.close();
        try
        {
            ret = std::stoi(s);
        }
        catch (const std::exception& e)
        {
            ret = -1;
        }
        return ret;
    }

    /*
     * writeSysFs: the method will write a number to sysfs file
     * inputs:
     * path - path to the file to write to
     * value - the value to write
     */

    int writeSysFs(std::string path, int value)
    {
        std::ofstream f(path.c_str());
        if (!f)
            return -1;
        f << value;
        f.close();
        return 0;
    }

    /*
     * enableArbitration:handling access to the Dragon arbitrator.
     * Dragon arbitrator is implemented in the kernel driver and is based of
     * Dragon CPLD specification. There is a dance with gpio's between the BMC
     * and the CPLD. In order to get access to the i2c bus the BMC must assert
     * the gpio and CPLD responds by asserting another gpio. It is possible for
     * the BMC to hold the bus for extended period of time by asserting the flag
     * to the driver. The driver will automatically pet the watchdog to the CPLD
     * if necessary. this is enabled by writing to the specific sysfs handle.
     * Arbitrator has timeoue of 20 minutes in case the BMC is stuck and the
     * driver is still petting the watchdog inputs: enable - enable/disable
     * boolean flag
     */
    int enableArbitration(bool enable)
    {
        int ret = 0;
        int error = static_cast<int>(UpdateError::ERROR_DISABLE_ARB);
        int value;
        int valueToWrite = 0;

        if (enable)
        {
            error = static_cast<int>(UpdateError::ERROR_ENABLE_ARB);
            valueToWrite = 1;
        }

        value = readSysFs(ARB_STATE_PATH);
        if (value == -1)
        {
            ret = error;
            goto end;
        }
        if (value != valueToWrite)
        {
            int cnt = 0;
            do
            {
                ret = writeSysFs(ARB_STATE_PATH, valueToWrite);
                if (ret == -1)
                {
                    ret = error;
                    goto end;
                }
                value = readSysFs(ARB_STATE_PATH);
                if (value == -1)
                {
                    ret = error;
                    goto end;
                }
                cnt++;
            } while (value != valueToWrite && cnt < arbTrials);

            if (value != valueToWrite)
            {
                ret = error;
                goto end;
            }
        }
    end:
        return ret;
    }

    /*
     * loadImage - loads image from imageName to image and set imageSize
     */
    int loadImage()
    {
        int loadFd = 0;
        struct stat st;
        int ret = 0;

        if (!imageName)
        {
            ret = static_cast<int>(UpdateError::ERROR_OPEN_IMAGE);
            goto end;
        }

        loadFd = open(imageName, O_RDONLY);
        if (loadFd < 0)
        {
            ret = static_cast<int>(UpdateError::ERROR_OPEN_IMAGE);
            goto end;
        }

        ret = stat(imageName, &st);
        if (ret)
        {
            ret = static_cast<int>(UpdateError::ERROR_STAT_IMAGE);
            goto close;
        }
        imageSize = st.st_size;
        if (imageSize <= 0)
        {
            ret = static_cast<int>(UpdateError::ERROR_STAT_IMAGE);
            goto close;
        }
        image = (char*)malloc(imageSize);
        if (!image)
        {
            ret = static_cast<int>(UpdateError::ERROR_LOAD_IMAGE);
            goto close;
        }

        if (read(loadFd, image, imageSize) != imageSize)
        {
            ret = static_cast<int>(UpdateError::ERROR_LOAD_IMAGE);
            goto close;
        }
    close:
        close(loadFd);
    end:
        return ret;
    }

    /*
     * sendData: sends data over i2c bus by using parent class methods
     * inputs:
     * send_fd - file descriptor for the i2c bus
     * send_addr - i2c address to send data to
     * data - pointer to data to send
     * length - length of data to send
     * error - which error should be returned in case of failure
     * returns: 0 for success, error code if failure
     */
    int sendData(int send_fd, int send_addr, uint8_t* data, int length,
                 int error)
    {
        uint8_t* dataWithPec =
            static_cast<uint8_t*>(malloc((length + 1) * sizeof(uint8_t)));

        if (!dataWithPec)
            return error;
        memcpy(dataWithPec, data, length);
        int ret = calculate_PEC(dataWithPec, address, length);

        if (ret)
        {
            ret = error;
            goto end;
        }

        ret = transfer(send_fd, 0, send_addr, dataWithPec, nullptr, length + 1,
                       0);
        if (ret)
        {
            ret |= error;
            goto end;
        }

    end:
        free(dataWithPec);
        return ret;
    }

    int sendDataWithRetry(int send_fd, int send_addr, uint8_t* data, int length,
                          int error, int retry)
    {
        int ret = 0;
        for (int i = 0; i < retry; i++)
        {
            ret = sendData(send_fd, send_addr, data, length, error);
            if (!ret)
                break;
        }
        return ret;
    }

    int transferWithRetry(int bus_fd, int is_read, uint32_t slave,
                          uint8_t* write_data, uint8_t* read_data,
                          int write_count, int read_count, int retry)
    {
        int ret = 0;
        for (int i = 0; i < retry; i++)
        {
            ret = transfer(bus_fd, is_read, slave, write_data, read_data,
                           write_count, read_count);
            if (!ret)
                break;
        }
        return ret;
    }

    /*
     * sendImage - virtual mehtod that will send the data to the target
     */
    virtual int sendImage() = 0;
};
