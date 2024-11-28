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

#include "utils.hpp"

#include <phosphor-logging/log.hpp>

#include <iostream>

void setProperty(const std::string& path, const std::string& interface,
                 const std::string& property, PropertyValue& val)
{
    auto bus = sdbusplus::bus::new_default();
    try
    {
        auto method = bus.new_method_call(service.c_str(), path.c_str(),
                                          "org.freedesktop.DBus.Properties",
                                          "Set");

        method.append(interface.c_str(), property.c_str(), val);
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
    }
}

void setFwVersion(const std::string& objPath, const std::string& version)
{
    PropertyValue ver(version);
    setProperty(objPath, "xyz.openbmc_project.Software.Version", "Version",
                ver);
}
