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

#pragma once

#include "cpld_util.hpp"

#include <filesystem>
#include <iostream>
#include <sdbusplus/bus/match.hpp>
#include <stdexcept>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

using namespace nvidia::cpld::common;

namespace nvidia::cpld::device
{

using CpldInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::
        OperationalStatus,
    sdbusplus::xyz::openbmc_project::Inventory::server::Item,
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis,
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset,
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Location,
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using AssociationObject = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using VersionObject = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;

using AssociationIfaceMap =
    std::map<std::string, std::unique_ptr<AssociationObject>>;

using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

using PropertyType = std::variant<std::string, bool>;

using Properties = std::map<std::string, PropertyType>;

class VersionInterface : public VersionObject
{
  public:
    /**
     * @brief Construct a new VersionInterfaceobject
     *
     * @param bus
     * @param path
     * @param fwversion
     */
    VersionInterface(sdbusplus::bus::bus& bus, const std::string& path,
                     std::string& fwversion) :
        VersionObject(bus, path.c_str(), action::emit_interface_added)
    {
        version(fwversion);
        purpose(VersionPurpose::Other);
    }
};

/**
 * @brief Association for dbus
 */
class Association : public AssociationObject
{
  public:
    /**
     * @brief Construct a Association object
     *
     * @param bus
     * @param path
     * @param fwAssociation
     */
    Association(sdbusplus::bus::bus& bus, const std::string& path,
                AssociationList& fwAssociation) :
        AssociationObject(bus, path.c_str(), action::emit_interface_added)
    {
        associations(fwAssociation);
    }
};

/**
 * @class Cpld
 */
class Cpld : public CpldInherit, public Util
{
  public:
    Cpld() = delete;
    Cpld(const Cpld&) = delete;
    Cpld(Cpld&&) = delete;
    Cpld& operator=(const Cpld&) = delete;
    Cpld& operator=(Cpld&&) = delete;
    ~Cpld() = default;

    Cpld(sdbusplus::bus::bus& bus, const std::string& objPath, uint8_t busN,
         uint8_t address, const std::string& name, const std::string& modelN,
         const std::string& manufacturerN, const std::string& assoc,
         const std::string& locationTypeN) :
        CpldInherit(bus, (objPath).c_str()),
        bus(bus)
    {

        b = busN;
        d = address;
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            manufacturer(manufacturerN);
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            model(modelN);
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            partNumber(getPartNumber());
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            serialNumber(getSerialNumber());
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::present(
            getPresence());
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::prettyName(
            "Cpld " + name);
        sdbusplus::xyz::openbmc_project::State::Decorator::server::
            OperationalStatus::functional(true);
        sdbusplus::xyz::openbmc_project::State::Decorator::server::
            OperationalStatus::state(
                sdbusplus::xyz::openbmc_project::State::Decorator::server::
                    OperationalStatus::StateType::Enabled);
        auto chassisType =
            sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis::
                convertChassisTypeFromString("xyz.openbmc_project.Inventory."
                                             "Item.Chassis.ChassisType.Module");
        sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis::type(
            chassisType);
        registerAssociationInterface(bus, objPath, assoc);
        registerSoftwareVersion(bus, objPath);
        if (!assoc.empty())
        {
            createAssociation(assoc);
        }
        if (!locationTypeN.empty())
        {
            auto locType =
                sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::
                    Location::convertLocationTypesFromString(locationTypeN);
            sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::
                Location::locationType(locType);
        }
    }

    void registerAssociationInterface(sdbusplus::bus::bus& bus,
                                      const std::string& ifPath,
                                      const std::string& assoc)
    {
        AssociationList fwAssociation;
        std::string swpath = SW_INV_PATH;
        std::string fName = std::filesystem::path(ifPath).filename().string();

        fName.insert(strlen(PLATFORM_PREFIX), PLATFORM_FW_PREFIX);
        swpath += "/" + fName;

        if (!assoc.empty())
        {
            fwAssociation.emplace_back(
                std::make_tuple("inventory", "activation", assoc));
        }

        fwAssociation.emplace_back(std::make_tuple(
            upFwdAssociation, upRevAssociation, softwareUpdateablePath));

        AssociationObj =
            std::make_unique<Association>(bus, swpath, fwAssociation);
    }
    void registerSoftwareVersion(sdbusplus::bus::bus& bus,
                                 const std::string& ifPath)
    {
        std::string swpath = SW_INV_PATH;
        std::string fName = std::filesystem::path(ifPath).filename().string();
        fName.insert(strlen(PLATFORM_PREFIX), PLATFORM_FW_PREFIX);
        swpath += "/" + fName;
        std::string psuVersion = getVersion();
        VersionObj =
            std::make_unique<VersionInterface>(bus, swpath, psuVersion);
    }

    const std::string& getInventoryPath() const
    {
        return inventoryPath;
    }

    /**
     * @brief Create a Association object
     *
     * @param reversePath
     */
    void createAssociation(const std::string& reversePath)
    {
        AssociationList assocs = {};
        ;
        assocs.emplace_back(
            std::make_tuple("parent_chassis", "inventory", reversePath));
        associations(assocs);
    }

  private:
    /** @brief systemd bus member */
    sdbusplus::bus::bus& bus;
    std::string inventoryPath;
    std::unique_ptr<VersionInterface> VersionObj;
    std::unique_ptr<Association> AssociationObj;
    const std::string upFwdAssociation = "software_version";
    const std::string upRevAssociation = "updateable";
    const std::string purpose =
        "xyz.openbmc_project.Software.Version.VersionPurpose.Other";
    const std::string softwareUpdateablePath = "/xyz/openbmc_project/software";
};

} // namespace nvidia::cpld::device
