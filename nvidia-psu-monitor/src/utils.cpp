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





#include "utils.hpp"
#include <iostream>
#include <unistd.h>

namespace nvidia::psumontior::utils {

// Get service name
const std::string DBusHandler::getService(const std::string &path,
                                          const std::string &interface) const {

  using InterfaceList = std::vector<std::string>;
  std::map<std::string, std::vector<std::string>> mapperResponse;

  auto &bus = DBusHandler::getBus();

  auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_OBJ_PATH,
                                    MAPPER_IFACE, "GetObject");
  mapper.append(path, InterfaceList({interface}));

  auto mapperResponseMsg = bus.call(mapper);
  mapperResponseMsg.read(mapperResponse);
  if (mapperResponse.empty()) {

    std::cerr << "Failed to read getService mapper response \n";
    return "";
  }

  // the value here will be the service name
  return mapperResponse.cbegin()->first;
}

// Set property
void DBusHandler::setProperty(const std::string &objectPath,
                              const std::string &interface,
                              const std::string &propertyName,
                              const PropertyValue &value) const {
  auto &bus = DBusHandler::getBus();
  auto service = getService(objectPath, interface);
  if (service.empty()) {
    return;
  }

  auto method = bus.new_method_call(service.c_str(), objectPath.c_str(),
                                    DBUS_PROPERTY_IFACE, "Set");
  method.append(interface.c_str(), propertyName.c_str(), value);

  bus.call_noreply(method);
}

// Get subtree paths

const std::vector<std::string>
DBusHandler::getSubTreePaths(const std::string &objectPath,
                             const std::string &interface) {
  std::vector<std::string> paths;

  auto &bus = DBusHandler::getBus();

  auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_OBJ_PATH,
                                    MAPPER_IFACE, "GetSubTreePaths");
  method.append(objectPath.c_str());
  method.append(0); // Depth 0 to search all
  method.append(std::vector<std::string>({interface.c_str()}));
  auto reply = bus.call(method);

  reply.read(paths);

  return paths;
}

} // namespace nvidia::psumontior::utils
