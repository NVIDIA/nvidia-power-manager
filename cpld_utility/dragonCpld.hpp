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

#include "dragonChassisBase.hpp"

#include <stdint.h>

#include <gpiod.hpp>

/*
 * class DragonCpld extends DragonChassisBase and implements fw update
 * process for Lattice CPLD present on Dragon chassis
 */
class DragonCpld : public DragonChassisBase
{
  public:
    /*
     * Constructor:
     * inputs:
     * cpldDevSel - index of CPLD that will be updated
     * imageName - path to image file
     * config - path to config file
     * config file is necessary due to the complexity of cpld firmware update
     * once cpld receives all the data activation command will be sent.
     * when this happens the underlying arb driver will decline all access to
     * the bus per cpld specification. Due to this communication to send refresh
     * command to the cpld must be done over raw (non-arbitrated version of
     * arbitrated bus) cpld is also communicating register access over another
     * i2c bus. to make process easier on upper layers whole config is expected
     * here.
     */
    DragonCpld(int cpldDevSel, char* imageName, const char* config);
    virtual ~DragonCpld();

    /*
     * fwUpdate - method that implements lattice fw update flow
     * the flow is documented in the lattice specification documents
     * document is availbale on lattice website but account must be
     * opened to obtain the documentation. the flow is:
     * enableConfigurationInterface->erase->sendImage->activate
     */
    int fwUpdate();

  protected:
    int bus;            // i2c bus number
    int cpldRegBus;     // i2c bus for the cpld register access
    int cpldRegAddress; // i2c address for the cpld register access
    int cpldRegFd;      // file descriptor for the cpld register access
    int cpldRawBus;     // non-arbitrated version of arbitrated bus
    int numOfPagesWritten;
    const char* config; // path to config file
    std::string devName;
    // this code is used on another program that does not use arbitration
    // in this case the refresh gpio must be handled in this code instead
    // of the arbitration driver
    gpiod::line cpldRefreshGpio;

    /*
     * enableConfigurationInterface matches expactation of Lattice CPLD update
     * flow in enable configuration stage
     */
    int enableConfigurationInterface();

    /*
     * erase matches expactation of Lattice CPLD update flow
     * in erase stage
     */
    int erase(bool reset);

    /*
     * activate matches expactation of Lattice CPLD update flow
     * in activate stage
     */
    int activate();
    int validate();

    /*
     * readDeviceId - reads ID of the underlying CPLD to make sure we are
     * updating correct device
     */
    int readDeviceId();

    /*
     * readFwVersion - reads FW version of the CPLD
     */
    std::string readFwVersion();

    /*
     * cleanUp matches expactation of Lattice CPLD update flow
     * in clean up stage
     */
    int cleanUp();

    /*
     * waitBusy: loops on cpld register waiting for the cpld to finish action
     * inputs:
     * wait - how long to wait for in seconds
     * errorCode - which error will be returned if failure happens
     */
    int waitBusy(int wait, int errorCode);

    /*
     * readStatusRegister: read status of cpld register
     * inputs:
     * value - pointer that will get the value of the register
     * returns error code. 0 for success error code for failure
     */
    int readStatusRegister(uint32_t* value);

    /* waitRefresh: waits for CPLD to finish refreshing the code
     * inputs:
     * result - expected value of the refresh sysfs handle
     */
    int waitRefresh(int result);

    /*
     * loadConfig: loads the configuration from the config file
     */
    int loadConfig();

    /*
     * sendImage - sends update image to the cpld
     */
    int sendImage() override;

    /**
     * handleHexString - Converts a string to an integer,
     *                   handling both decimal and hexadecimal formats.
     *
     * @param str The input string to be converted.
     *            If the string starts with "0x", it's treated as hexadecimal.
     *            Otherwise, it's treated as decimal.
     *
     * @return The converted integer value.
     *         For hexadecimal input, the result is left-shifted by 1 bit.
     */
    int handlHexString(std::string str)
    {
        int address;
        // treat it as hex format if it has 0x prefix
        if (str.substr(0, 2) == "0x")
        {
            str = str.substr(2);
            address = stoi(str, nullptr, 16);
            address <<= 1;
        }
        else
        {
            address = stoi(str);
        }
        return address;
    }
};
