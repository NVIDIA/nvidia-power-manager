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





#pragma once
#include "power_manager_property.hpp"
using namespace phosphor::logging;

namespace nvidia::power::manager {

#define DBUSTYPE_SIGNED_INT16 'n'
#define DBUSTYPE_UNSIGNED_INT16 'q'
#define DBUSTYPE_SIGNED_INT32 'i'
#define DBUSTYPE_UNSIGNED_INT32 'u'
#define DBUSTYPE_SIGNED_INT64 'x'
#define DBUSTYPE_UNSIGNED_INT64 't'
#define DBUSTYPE_DOUBLE 'd'
#define DBUSTYPE_BYTE 'y'
#define DBUSTYPE_STRING 's'
#define DBUSTYPE_BOOL 'b'
#define DBUSTYPE_DICT 'e'

using Value =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, std::vector<uint8_t>,
                 std::vector<int16_t>, std::vector<uint16_t>,
                 std::vector<int32_t>, std::vector<uint32_t>,
                 std::vector<int64_t>, std::vector<uint64_t>,
                 std::vector<double>, std::vector<std::string>>;

/**
 * @class PowerManager
 *
 * This class will create an object used to manage and monitor Power redundancy
 * and power capping
 *
 */
class PowerManager {
public:
  PowerManager() = delete;
  ~PowerManager() = default;
  PowerManager(const PowerManager &) = delete;
  PowerManager &operator=(const PowerManager &) = delete;
  PowerManager(PowerManager &&) = delete;
  PowerManager &operator=(PowerManager &&) = delete;

  /**
   * Constructor to read the configuration file and create the Power capping
   * properties and register watch call back functions for any changes in the
   * PSU sensors and power Capping property state changes.
   *
   * @param[in] bus - D-Bus bus object
   * @param[in] objectServer - event object
   */
  PowerManager(sdbusplus::bus::bus &bus,
               sdbusplus::asio::object_server &objectServer);

private:
  /**
   * The D-Bus object
   */
  sdbusplus::bus::bus &bus;

  /**
   * The json object of the parsed Power configuration Json file
   */
  nlohmann::json JsonConfigData;

  sdbusplus::asio::object_server &objServer;

  uint32_t curentPowerLimit;

  std::string chassisObjectPath;

  /** @brief True if the power is on. */
  bool powerOn = false;

  /** @brief Used to parse the input paramaters data type from json and append
   * to method call object
   *
   * @param[in] jsonData - Json data containing Action block parameters
   * @param[in] methodObj - sdbus message object to which input data parameters
   * are appended in order to issue method call
   *
   */
  void getDataForAppend(nlohmann::json dataJson,
                        sdbusplus::message::message &methodObj);

  /** @brief Used to subscribe to D-Bus events and property state changes */
  std::vector<std::unique_ptr<sdbusplus::bus::match_t>> matchEvent;

  /** @brief Used to create Property objects to register Power Capping
   * Properties */
  std::vector<std::unique_ptr<property::Property>> propertyObjs;

  std::vector<std::unique_ptr<property::areaObject>> areaObjs;

  /** @brief Used to subscribe to D-Bus power state changes */
  std::unique_ptr<sdbusplus::bus::match_t> currentPowerState;

  /** @brief Used to store Dbus interface objects Power Capping Properties */
  std::vector<std::shared_ptr<sdbusplus::asio::dbus_interface>>
      enabledInterface;

  std::string getSystemChassisObjectPath();

  /** @brief Used to update Global Power Capping Properties structure from the
   * power manager configuration
   *
   * @param[in] fileExist - if true, update database from file
   * otherwise update database with default values
   *
   */
  void updatePowerCappingStructure(bool fileExist);

  /** @brief Used to update Global Power Capping Propery of GPU the power
   * manager configuration*/
  void updatePowerCappingLimit(bool emitsChange);

  void updatePowerModePropertyValue(uint8_t mode);

  /** @brief Used to update PowerCap property based on Mode
   *
   * @param[in] jsonData - Json data containing Action block parameters
   * @param[in] propertyName - property name which triggered the action block
   *
   */
  template <typename T>
  void executeActionBlock(nlohmann::json jsonData, std::string propertyName,
                          T &state);

  /** @brief Used to update PowerCap property based on Mode
   *
   * @param[in] jsonData - Json data containing Condition block parameters
   * @return[out] - true/false
   *
   */
  bool executeConditionBlock(nlohmann::json jsonData);
  /**
   * @brief Callback for power redundancy sensors state changes
   *
   * Process carries out the recovery action and sel logging for redundancy
   * sensors for the system.
   * when the function call back is called it go through the json data and
   * perform the Action blocks defined in the powermanager.json
   *
   * @param[in] msg - Data associated with the state signal
   */
  void EventTriggered(sdbusplus::message::message &msg);

  /**
   * @brief Callback for power capping properties state changes
   *
   * Process carries out the sel logging and capping action for capping
   * properties of the system.
   * when the function call back is called it go through the json data and
   * perform the Action blocks defined in the powermanager.json
   *
   * @param[in] iface - D-Bus interface name
   * @param[in] path - D-Bus object path name
   * @param[in] var - The configured data 
   */
  void PropertyTriggered(std::string iface, std::string path,
                         std::variant<uint32_t, std::string, nvidia::power::manager::property::PowerMode > var);

  /**
   * @brief Callback for power state changes
   *
   * Process carries out the sel logging and capping action when Power changes
   * to on status.
   * when the function call back is called it go through the json data and
   * perform the Action blocks defined in the powermanager.json
   *
   * @param[in] msg - Data associated with the state signal
   */
  void powerStateTriggered(sdbusplus::message::message &msg);

  /** @brief Used to update PowerCap property based on Mode
   *
   * @param[in] mode - PowerMode string
   *
   */
  void updatePowerCapPropertyValue(std::string mode);

  /** @brief Handler to add OEM specific Parameters to the Dbus Method Calls of
   * action/conditon Blocks */
  void oemKeyHandler(sdbusplus::message::message &methodObj,
                     std::string propertyName, std::string key);

  /** @brief Used to Calculate the checksum of data buffer
   *
   * @param[in] data - data buffer
   * @param[in] length - length of the data buffer
   *
   */
  uint8_t caluclateChecksum(uint8_t *data, size_t length);

  /** @brief Used to validate the checksum of data buffer
   *
   * @param[in] data - data buffer
   * @param[in] length - length of the data buffer
   *
   * @return[out] - true/false
   *
   */
  bool checkChecksum(uint8_t *data, size_t length);

  /** @brief Triggers Emit change on Property */
  void triggerSystemPowerCapSignal();

  /** @brief structure object which holds power capping information. */
  struct PowerCappingInfo powerCappingInfo;
};

} // namespace nvidia::power::manager
