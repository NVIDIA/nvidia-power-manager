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





#include "cpld_manager.hpp"

#include <fmt/format.h>
#include <sys/types.h>
#include <unistd.h>

#include <regex>
#include <xyz/openbmc_project/Common/Device/error.hpp>

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Device::Error;
using namespace nvidia::cpld::common;

namespace nvidia::cpld::manager
{

CPLDManager::CPLDManager(sdbusplus::bus::bus& bus, std::string basePath) :
    bus(bus)
{
    using namespace sdeventplus;

    nlohmann::json fruJson = loadJSONFile(CPLD_JSON_PATH);
    if (fruJson == nullptr)
    {
        log<level::ERR>("InternalFailure when parsing the JSON file");
        return;
    }
    for (const auto& fru : fruJson.at("CPLD"))
    {
        try
        {
            const auto baseinvInvPath =
                basePath + "/" + PLATFORM_PREFIX + "CPLD_";

            std::string id = fru.at("Index");
            std::string busN = fru.at("Bus");
            std::string address = fru.at("Address");
            std::string model = fru.at("Model");
            std::string manufacturer = fru.at("Manufacturer");
            std::string invpath = baseinvInvPath + id;
            std::string assoc = "";

            if (fru.contains("ChassisAssociationEndpoint"))
            {
                assoc = fru.at("ChassisAssociationEndpoint");
            }

            uint8_t busId = std::stoi(busN);
            uint8_t devAddr = std::stoi(address, nullptr, 16);

            auto invMatch = std::find_if(
                cpldInvs.begin(), cpldInvs.end(), [&invpath](auto& inv) {
                    return inv->getInventoryPath() == invpath;
                });
            if (invMatch != cpldInvs.end())
            {
                continue;
            }
            auto inv = std::make_unique<Cpld>(bus, invpath, busId, devAddr, id,
                                              model, manufacturer, assoc);
            cpldInvs.emplace_back(std::move(inv));
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

} // namespace nvidia::cpld::manager
