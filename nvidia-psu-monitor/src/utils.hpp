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
#include <sdbusplus/server.hpp>

#include <map>
#include <vector>

namespace nvidia::psumontior::utils {

constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_OBJ_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_IFACE = "xyz.openbmc_project.ObjectMapper";
constexpr auto DBUS_PROPERTY_IFACE = "org.freedesktop.DBus.Properties";

// The value of the property(type: variant, contains some basic types)
// Eg: uint8_t : dutyOn, uint16_t : Period, std::string : Name,
// std::vector<std::string> : endpoints, bool : Functional
using PropertyValue = std::variant<uint8_t, uint16_t, std::string,
                                   std::vector<std::string>, bool>;

// The name of the property
using DbusProperty = std::string;

// The Map to constructs all properties values of the interface
using PropertyMap = std::map<DbusProperty, PropertyValue>;

/**
 *  @class DBusHandler
 *
 *  Wrapper class to handle the D-Bus calls
 *
 *  This class contains the APIs to handle the D-Bus calls.
 */
class DBusHandler {
public:
  /** @brief Get the bus connection. */
  static auto &getBus() {
    static auto bus = sdbusplus::bus::new_default();
    return bus;
  }

  /**
   *  @brief Get service name by the path and interface of the DBus.
   *
   *  @param path
   *  @param interface
   *
   *  @return std::string
   *
   */
  const std::string getService(const std::string &path,
                               const std::string &interface) const;

  /** @brief Set D-Bus property
   *
   *  @param objectPath
   *  @param interface
   *  @param propertyName
   *  @param value
   *
   *  @throw sdbusplus::exception::exception when it fails
   */
  void setProperty(const std::string &objectPath, const std::string &interface,
                   const std::string &propertyName,
                   const PropertyValue &value) const;

  /** @brief Get sub tree paths by the path and interface of the DBus.
   *
   *  @param  objectPath
   *  @param  interface
   *  @return std::vector<std::string> vector of subtree paths
   */
  const std::vector<std::string> getSubTreePaths(const std::string &objectPath,
                                                 const std::string &interface);
};

} // namespace nvidia::psumontior::utils
