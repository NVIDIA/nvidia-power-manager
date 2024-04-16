/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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





#include "PsuMonitor.hpp"
#include "utils.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sys/ioctl.h>

#include <bitset>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <iostream>
#include <unistd.h>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

namespace nvidia::psumonitor {

int PsuMonitor::getPsuConfigValues() {

  int ret = 0;
  try {
    std::ifstream psu_json_file("/usr/share/nvidia-power-manager/psu.json");
    if (!psu_json_file.good()) {
      std::cerr << "file open failed exiting from psumonitor service \n";
      return -1;
    }

    json psuConfigJson; // json object for psu config
    psu_json_file >> psuConfigJson;

    detectRegAddr =
        psuConfigJson["psu_config"]["PSU_DETECT_N"]["RegisterAddress"];
    alertRegAddr =
        psuConfigJson["psu_config"]["PSU_ALERT_N"]["RegisterAddress"];
    workRegAddr = psuConfigJson["psu_config"]["PSU_WORK_N"]["RegisterAddress"];
    psuDropRegAddr =
        psuConfigJson["psu_config"]["PSU_EVENT"]["RegisterAddress"];

    bus = psuConfigJson["psu_config"]["i2c"]["Bus"];
    slaveAddress = psuConfigJson["psu_config"]["i2c"]["SlaveAddress"];
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    ret = -1;
  }

  return ret;
}

inline std::string PsuMonitor::getPowerStateEnumeration(bool power) {
  if (power)
    return "xyz.openbmc_project.State.Decorator.PowerState.State.On";
  else
    return "xyz.openbmc_project.State.Decorator.PowerState.State.Off";
}

void PsuMonitor::udpateProperty(const std::string &objectPath,
                                const std::string &iface,
                                const std::string &propertyName,
                                const bool value) {

  try {
    if (iface == powerStateIface) {
      const std::string currentPowerState = getPowerStateEnumeration(value);
      PropertyValue state{currentPowerState};
      dBusHandler.setProperty(objectPath, iface, propertyName, state);
    } else {
      PropertyValue bVal{value};
      dBusHandler.setProperty(objectPath, iface, propertyName, bVal);
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return;
}

int32_t PsuMonitor::i2cRead(const int &busId, const int &slaveAddr,
                            const int32_t &statusReg) {
  std::string i2cBus = "/dev/i2c-" + std::to_string(busId);

  int fd = open(i2cBus.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    std::cerr << "unable to open i2c device \n";
    return -1;
  }
  if (ioctl(fd, I2C_SLAVE_FORCE, slaveAddr) < 0) {
    std::cerr << "unable to set device address\n";
    close(fd);
    return -1;
  }

  unsigned long funcs = 0;
  if (ioctl(fd, I2C_FUNCS, &funcs) < 0) {
    std::cerr << "not support I2C_FUNCS \n";
    close(fd);
    return -1;
  }

  if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
    std::cerr << "not support I2C_FUNC_SMBUS_READ_BYTE_DATA \n";
    close(fd);
    return -1;
  }

  int32_t statusValue = i2c_smbus_read_byte_data(fd, statusReg);

  close(fd);

  if (statusValue < 0) {
    std::cerr << "i2c_smbus_read_byte_data failed \n";
    return -1;
  }

  return statusValue;
}

int PsuMonitor::update_status(const int32_t &detectNewVal,
                              const int32_t &alertNewVal,
                              const int32_t &workNewVal,
                              const int32_t &DropNewVal, int initialize = 0) {

  int psus[]{0, 1, 2, 3, 4, 5};
  int eve[]{0, 1, 2};
  int rc = 0;

  try {
    std::bitset<6> psuDetectNewArr(detectNewVal);
    std::bitset<6> psuAlertNewArr(alertNewVal);
    std::bitset<6> psuWorkNewArr(workNewVal);
    std::bitset<8> psuDropNewArr(DropNewVal);

    if ((detectNewVal != detectRegValue) || (initialize == 1)) {

      for (const auto &i : psus) {
        for (const auto &psuObjectPath : psuObjectPaths) {
          if (psuObjectPath.find("powersupply" + std::to_string(i)) !=
              std::string::npos) {
            udpateProperty(psuObjectPath, INVENTORY_IFACE, present,
                           !psuDetectNewArr[i]);
          }
        }
      }
      detectRegValue = detectNewVal;
    }

    if ((alertRegValue != alertNewVal) || (initialize == 1)) {

      for (const auto &i : psus) {
        for (const auto &psuObjectPath : psuObjectPaths) {
          if (psuObjectPath.find("powersupply" + std::to_string(i)) !=
              std::string::npos) {
            udpateProperty(psuObjectPath, powerStateIface, powerState,
                           !psuAlertNewArr[i]);
          }
        }
      }
      alertRegValue = alertNewVal;
    }

    if ((workRegValue != workNewVal) || (initialize == 1)) {

      for (const auto &i : psus) {
        for (const auto &psuObjectPath : psuObjectPaths) {
          if (psuObjectPath.find("powersupply" + std::to_string(i)) !=
              std::string::npos) {
            udpateProperty(psuObjectPath, operationalIface, functional,
                           !psuWorkNewArr[i]);
          }
        }
      }
      workRegValue = workNewVal;
    }

    if ((psuDropRegValue != DropNewVal) || (initialize == 1)) {

      for (const auto &i : eve) {
        if (psuDropNewArr[i + eveOffset]) {
          psuEvents[i]->enabledInterface->set_property("Enabled", true);
        } else {
          psuEvents[i]->enabledInterface->set_property("Enabled", false);
        }
      }
      psuDropRegValue = DropNewVal;
    }

  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    rc = -1;
  }

  return rc;
}

void PsuMonitor::pollRegisters() {

  // setting a new experation implicitly cancels any pending async wait
  mPollTimer.expires_from_now(
      boost::posix_time::seconds(intrusionSensorPollSec));

  mPollTimer.async_wait([&](const boost::system::error_code &ec) {
    // case of timer expired
    if (!ec) {

      auto detectRegValueCur = i2cRead(bus, slaveAddress, detectRegAddr);
      if (detectRegValueCur < 0) {

        std::cerr << "Error: i2c Read for detect Register Failed\n";
      }

      auto alertRegValueCur = i2cRead(bus, slaveAddress, alertRegAddr);
      if (alertRegValueCur < 0) {

        std::cerr << "Error: i2c Read for Alert Register Failed\n";
      }

      auto workRegValueCur = i2cRead(bus, slaveAddress, workRegAddr);
      if (workRegValueCur < 0) {

        std::cerr << "Error: i2c Read for Work Register Failed\n";
      }

      auto psuDropRegValueCur = i2cRead(bus, slaveAddress, psuDropRegAddr);
      if (psuDropRegValueCur < 0) {

        std::cerr << "Error: i2c Read for PSU Drop Register Failed\n";
      }

      if ((detectRegValueCur != detectRegValue) ||
          (alertRegValueCur != alertRegValue) ||
          (workRegValueCur != workRegValue) ||
          (psuDropRegValueCur != psuDropRegValue)) {

        auto ret = update_status(detectRegValueCur, alertRegValueCur,
                                 workRegValueCur, psuDropRegValueCur);
        if (ret < 0) {

          std::cerr << " update_status failed \n";
        }
      }

      // trigger next polling
      pollRegisters();
    }
    // case of being canceled
    else if (ec == boost::asio::error::operation_aborted) {

      std::cerr << "Timer  cancelled. Return \n";
      return;
    }
  });
}

/**
 * @brief start function for psu monitoring
 *
 */
void PsuMonitor::start() {

  int initProperties = 1;
  psuObjectPaths = dBusHandler.getSubTreePaths("/", powerSupplyIface);

  if (psuObjectPaths.empty()) {

    std::cerr << "Error: Get PSU Object Paths Failed \n";
    return;
  }

  std::sort(psuObjectPaths.begin(), psuObjectPaths.end());
  psuEveObjectPaths = {"/xyz/openbmc_project/sensors/power/psu_drop_to_1_event",
                       "/xyz/openbmc_project/sensors/power/psu_drop_to_2_event",
                       "/xyz/openbmc_project/sensors/power/MBON_GBOFF_event"};

  if (getPsuConfigValues() == -1) {

    std::cerr << "Error: Parsing Json Configuration File \n";
    return;
  }

  detectRegValue = i2cRead(bus, slaveAddress, detectRegAddr);
  if (detectRegValue < 0) {
    std::cerr << "Error: i2c Read detect Failed\n";
  }
  alertRegValue = i2cRead(bus, slaveAddress, alertRegAddr);
  if (alertRegValue < 0) {
    std::cerr << "Error: i2c Read alert Failed\n";
  }

  workRegValue = i2cRead(bus, slaveAddress, workRegAddr);
  if (workRegValue < 0) {
    std::cerr << "Error: i2c Read work Failed\n";
  }

  psuDropRegValue = i2cRead(bus, slaveAddress, psuDropRegAddr);
  if (psuDropRegValue < 0) {
    std::cerr << "Error: i2c Read drop Failed\n";
  }

  auto ret = update_status(detectRegValue, alertRegValue, workRegValue,
                           psuDropRegValue, initProperties);
  if (ret < 0) {

    std::cerr << " init update_status failed \n";
    return;
  }

  pollRegisters();
  return;
}

PsuMonitor::PsuMonitor(boost::asio::io_service &io,
                       std::vector<std::shared_ptr<PsuEvent>> &pEvents)
    : psuEvents(pEvents), mPollTimer(io), bus(-1), slaveAddress(-1),
      detectRegAddr(-1), alertRegAddr(-1), workRegAddr(-1), psuDropRegAddr(-1),
      detectRegValue(-1), alertRegValue(-1), workRegValue(-1),
      psuDropRegValue(-1) {}

PsuMonitor::~PsuMonitor() { mPollTimer.cancel(); }

} // namespace nvidia::psumonitor
