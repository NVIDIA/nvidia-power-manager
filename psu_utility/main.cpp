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

#include <algorithm>
#include <cstdint> // uint8_t (8 bits) and uint16_t (16 bits)
#include <cstdlib> // <stdlib.h>
#include <cstring> // <string.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

//------ Poxix/ U-nix specific header files ------//
#include "deltapsuxx.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_RETRY 3
#define DELAY_100_US 100
namespace fs = std::filesystem;

int unTar(const std::string& tarFilePath, const std::string& extractDirPath)
{
    if (tarFilePath.empty())
    {
        cerr << "TarFilePath is empty - " << tarFilePath.c_str() << endl;
        return -1;
    }
    if (extractDirPath.empty())
    {
        cerr << "ExtractDirPath is empty - " << extractDirPath.c_str() << endl;
        return -1;
    }

    VERBOSE << "Untaring {PATH} to {EXTRACTIONDIR}"
            << " PATH -" << tarFilePath.c_str() << " EXTRACTIONDIR- "
            << extractDirPath.c_str() << endl;
    int status = 0;
    pid_t pid = fork();

    if (pid == 0)
    {
        // child process
        execl("/bin/tar", "tar", "-xf", tarFilePath.c_str(), "-C",
              extractDirPath.c_str(), (char*)0);
        // execl only returns on fail
        cerr << "Failed to execute untar on " << tarFilePath << endl;
        return -1;
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status))
        {
            cerr << "Failed to execute untar on " << tarFilePath.c_str()
                 << " STATUS - " << status << endl;
            return -1;
        }
    }
    else
    {
        cerr << "Failed to execute untar on " << tarFilePath.c_str()
             << " ERRNO - " << errno << endl;
        return -1;
    }

    return 0;
}

int file_lock(int fd)
{
    /* Lock the file from beginning to end, blocking if it is already locked */
    struct flock lock, savelock;
    lock.l_type = F_WRLCK; /* Test for any lock on any part of file. */
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    savelock = lock;
    fcntl(fd, F_GETLK, &lock); /* Overwrites lock structure with preventors. */
    if (lock.l_type == F_WRLCK)
    {
        cerr << "Process " << lock.l_pid << " has a write lock already!"
             << endl;
        exit(1);
    }
    else if (lock.l_type == F_RDLCK)
    {
        cerr << "Process " << lock.l_pid << " has a read lock already!" << endl;
        exit(1);
    }
    return fcntl(fd, F_SETLK, &savelock);
}

int file_unlock(int fd)
{
    struct flock lock, savelock;
    lock.l_type = F_UNLCK; /* Test for any lock on any part of file. */
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    savelock = lock;

    fcntl(fd, F_GETLK, &lock); /* Overwrites lock structure with preventors. */
    if (lock.l_type == F_WRLCK || lock.l_type == F_RDLCK)
    {
        cerr << "Process " << lock.l_pid << " has a write lock already!"
             << endl;
        return fcntl(fd, F_SETLK, &savelock);
    }
    else
    {
        cerr << "Process " << lock.l_pid << " lock.l_type - " << lock.l_type
             << " has not been locked !" << endl;
        return 0;
    }
}

//---------- Main Code -------------------------------//
#ifdef STANDALONE_UTILITY
int main(int argc, char** argv)
{
    const char* programName = argv[0];
    int bus, device_slaveaddr;
    string cmd;
    fs::path tarImageFilename;
    int fd = -1, i = 0, retry = 0, ret = 0;
    auto printUsage = [programName]() {
        std::cerr << "Usage: " << std::endl;
        std::cerr << programName << " version [bus] [slave address] "
                  << std::endl;
        std::cerr << programName
                  << " fwupgrade [bus] [slave address] [ Tar image file]  "
                  << std::endl;
        std::cerr << programName << " activate [bus] [slave address]  "
                  << std::endl;
        std::cerr << "options: " << std::endl;
        std::cerr << " -v  verbose " << std::endl;
    };

    int opt;

    // put ':' in the starting of the
    // string so that program can
    // distinguish between '?' and ':'
    while ((opt = getopt(argc, argv, "v")) != -1)
    {
        switch (opt)
        {
            case 'v':
                verbose = 1;
                VERBOSE << "Verbose mode started" << endl;
                break;
        }
    }

    if (optind >= argc)
    {
        printUsage();
        cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
        return -(static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS));
    }

    VERBOSE << "argc=" << argc << endl;
    VERBOSE << "optind=" << optind << endl;
    if (argc < 4)
    {
        printUsage();
        cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
        return -(static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS));
    }
    cmd = string(argv[optind]);
    bus = stoi(argv[optind + 1], nullptr, 0);
    device_slaveaddr = stoi(argv[optind + 2], nullptr, 0);

#else
int componentSendCommand(string cmd, int bus, int device_slaveaddr,
                         fs::path tarImageFilename)
{
    int fd = -1, i = 0, retry = 0, ret = 0;
#endif

    nlohmann::json fruJson =
        loadJSONFile("/usr/share/nvidia-power-manager/psu_config.json");
    if (fruJson == nullptr)
    {
        log<level::ERR>("InternalFailure when parsing the JSON file");
        return -(static_cast<int>(ErrorStatus::ERROR_UNKNOW));
    }
    for (const auto& fru : fruJson.at("PowerSupplies"))
    {
        if (fru.at("I2cBus") == bus &&
            fru.at("I2cSlaveAddress") == device_slaveaddr)
        {
            psuId = fru.at("Index");
        }
    }
    fd = I2c::openBus(bus);
    if (fd < 0)
    {
        cout << "Error opening i2c file: " << strerror(errno) << endl;
        return -(static_cast<int>(ErrorStatus::ERROR_OPEN_I2C_DEVICE));
    }
    for (i = 0; i < MAX_RETRY; i++)
    {
        /* Get shared lock */
        if ((ret = file_lock(fd)) >= 0)
        {
            VERBOSE << "Process got a read lock already!  " << endl;
            break;
        }
        else if (i == MAX_RETRY)
        {
            cerr << __func__ << ":" << __LINE__ << "-"
                 << "ERROR: Getting lock Failed!!!" << endl;
            close(fd);
            return -EXIT_FAILURE;
        }
        usleep(DELAY_100_US);
    }
    cout << " Firmware update issued on [bus]= " << bus
         << " [slave address]= " << device_slaveaddr << endl;
    DeltaPsu device(fd, device_slaveaddr);
    if (cmd == "version")
    {
        VERBOSE << "Retriving version number" << endl;
        device.fwversion();
    }
    else if (cmd == "fwupgrade")
    {
#ifdef STANDALONE_UTILITY
        VERBOSE << "argc=" << argc << endl;
        if (argc < 5)
        {
            printUsage();
            cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
            file_unlock(fd);
            close(fd);
            return -(static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS));
        }
        tarImageFilename = argv[optind + 3];
#endif
        if (!fs::exists(tarImageFilename))
        {
            file_unlock(fd);
            close(fd);
            return -(static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS));
        }
        if (unTar(tarImageFilename.string(),
                  tarImageFilename.parent_path().string()) < 0)
        {
            file_unlock(fd);
            close(fd);
            return -(static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS));
        }
        vector<pair<int, fs::path>> imageParameters;
        fs::path extractedDirectory;
        for (const auto& dir_entry :
             fs::recursive_directory_iterator(tarImageFilename.parent_path()))
        {
            if (dir_entry.is_regular_file())
            {
                std::cout << dir_entry.path().filename() << '\n';
                if (dir_entry.path().filename().string().find("Pri") !=
                    std::string::npos)
                {
                    imageParameters.push_back(make_pair(0, dir_entry.path()));
                }
                else if (dir_entry.path().filename().string().find("Sec") !=
                         std::string::npos)
                {
                    imageParameters.push_back(make_pair(1, dir_entry.path()));
                }
                else if (dir_entry.path().filename().string().find("Com") !=
                         std::string::npos)
                {
                    imageParameters.push_back(make_pair(2, dir_entry.path()));
                }
            }
            else if (dir_entry.is_directory())
            {
                extractedDirectory = dir_entry.path();
            }
        }

        for (const auto& image_entry : imageParameters)
        {
            retry = 0;
            do
            {
                VERBOSE << "starting fwupgrade process " << endl;
                std::map<std::string, std::string> addData;
                addData["REDFISH_MESSAGE_ID"] = trargetDetermined;
                addData["REDFISH_MESSAGE_ARGS"] =
                    ("PSU" + psuId + "," + image_entry.second.string());
                // use separate container for fwupdate message registry
                addData["namespace"] = "FWUpdate";
                Level level = Level::Informational;
                createLog(trargetDetermined, addData, level);
                ret = device.fwupdate(image_entry.second.c_str(),
                                      image_entry.first);
                if (ret == 0)
                {
                    addData["REDFISH_MESSAGE_ID"] = updateSuccessful;
                    addData["REDFISH_MESSAGE_ARGS"] =
                        ("PSU" + psuId + "," + image_entry.second.string());
                    // use separate container for fwupdate message registry
                    addData["namespace"] = "FWUpdate";
                    level = Level::Informational;
                    createLog(updateSuccessful, addData, level);
                    addData["REDFISH_MESSAGE_ID"] = awaitToActivate;
                    addData["REDFISH_MESSAGE_ARGS"] =
                        (image_entry.second.string() + "," + "PSU" + psuId);
                    // use separate container for fwupdate message registry
                    addData["namespace"] = "FWUpdate";
                    level = Level::Informational;
                    createLog(awaitToActivate, addData, level);
                    break;
                }
                else if (retry > MAX_RETRY)
                {
                    VERBOSE << "retry= " << retry << endl;
                    addData["REDFISH_MESSAGE_ID"] = applyFailed;
                    addData["REDFISH_MESSAGE_ARGS"] =
                        (image_entry.second.string() + "," + "PSU" + psuId);
                    // use separate container for fwupdate message registry
                    addData["namespace"] = "FWUpdate";
                    level = Level::Critical;
                    createLog(applyFailed, addData, level);
                    ret = -(static_cast<int>(ErrorStatus::ERROR_UPG_FAIL));
                    break;
                }
                else
                {
                    addData["REDFISH_MESSAGE_ID"] = applyFailed;
                    addData["REDFISH_MESSAGE_ARGS"] =
                        (image_entry.second.string() + "," + "PSU" + psuId);
                    // use separate container for fwupdate message registry
                    addData["namespace"] = "FWUpdate";
                    level = Level::Critical;
                    createLog(applyFailed, addData, level);

                    retry++;
                }
            } while (1);
        }

        if (fs::exists(extractedDirectory))
        {
            fs::remove_all(extractedDirectory);
        }
    }
    else if (cmd == "activate")
    {
        VERBOSE << "Activate the device" << endl;
        device.activate();
    }
    else
    {
#ifdef STANDALONE_UTILITY
        printUsage();
        cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
#endif
        file_unlock(fd);
        close(fd);
        return -(static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS));
    }
    if (fd != -1)
    {
        file_unlock(fd);
        close(fd);
    }

    return EXIT_SUCCESS;
} // --- End of main() --- //
