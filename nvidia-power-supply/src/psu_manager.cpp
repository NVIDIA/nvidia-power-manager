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

#include "psu_manager.hpp"

#include <fmt/format.h>
#include <sys/types.h>
#include <unistd.h>

#include <xyz/openbmc_project/Common/Device/error.hpp>

#include <fstream>
#include <iostream>
#include <regex>

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Device::Error;
using namespace nvidia::power::common;

namespace nvidia::power::manager
{

PSUManager::PSUManager(sdbusplus::bus::bus& bus, std::string baseInvPath) :
    bus(bus), baseInventoryPath(baseInvPath)
{

    using namespace sdeventplus;

    nlohmann::json fruJson = loadJSONFile(PSU_JSON_PATH);
    if (fruJson == nullptr)
    {
        log<level::ERR>("InternalFailure when parsing the JSON file");
        return;
    }
    std::string cmdUtilityName = "psui2ccmd.sh";
    try
    {
        cmdUtilityName = fruJson.at("CommandName");
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    for (const auto& fru : fruJson.at("PowerSupplies"))
    {
        try
        {
            std::string id = fru.at("Index");
            std::string assoc = fru.at("ChassisAssociationEndpoint");
            std::string invpath = baseInventoryPath + "/powersupply" + id;
            auto invMatch =
                std::find_if(psus.begin(), psus.end(), [&invpath](auto& psu) {
                    return psu->getInventoryPath() == invpath;
                });
            if (invMatch != psus.end())
            {
                continue;
            }

            auto psu = std::make_unique<PowerSupply>(bus, invpath,
                                                     cmdUtilityName, id, assoc);
            psus.emplace_back(std::move(psu));
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

} // namespace nvidia::power::manager
