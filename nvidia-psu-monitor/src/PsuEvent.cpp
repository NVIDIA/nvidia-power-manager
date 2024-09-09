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

#include "PsuEvent.hpp"

#include <unistd.h>

#include <iostream>
#include <string>

namespace nvidia::psumonitor::event
{
/**
 * @class PsuEvent
 */
PsuEvent::PsuEvent(const std::string& name,
                   sdbusplus::asio::object_server& objectServer) :
    objServer(objectServer),
    name(std::move(name))
{
    enabledInterface =
        objServer.add_interface("/xyz/openbmc_project/sensors/power/" + name,
                                "xyz.openbmc_project.Object.Enable");

    enabledInterface->register_property(
        "Enabled", true, sdbusplus::asio::PropertyPermission::readWrite);

    if (!enabledInterface->initialize())
    {
        std::cerr << "error initializing enabled interface\n";
    }
}

PsuEvent::~PsuEvent()
{
    objServer.remove_interface(enabledInterface);
}

} // namespace nvidia::psumonitor::event
