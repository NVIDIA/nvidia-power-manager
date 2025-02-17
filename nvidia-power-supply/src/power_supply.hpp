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

#include "psu_util.hpp"

#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PowerSupply/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>
#include <xyz/openbmc_project/State/Decorator/PowerState/server.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace nvidia::power::common;

namespace nvidia::power::psu
{

using PowerSupplyInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::
        OperationalStatus,
    sdbusplus::xyz::openbmc_project::State::Decorator::server::PowerState,
    sdbusplus::xyz::openbmc_project::Inventory::server::Item,
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PowerSupply,
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset,
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::LocationCode,
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using PowerSupplySensorInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::
        OperationalStatus,
    sdbusplus::xyz::openbmc_project::State::Decorator::server::PowerState,
    sdbusplus::xyz::openbmc_project::Inventory::server::Item,
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PowerSupply>;

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

/**
 * @brief VersionInterface for dbus
 */
class VersionInterface : public VersionObject
{
  public:
    /**
     * @brief Construct a new VersionInterface object
     *
     * @param bus
     * @param path
     * fwversion
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
 * @brief PowerSupplySensor for dbus
 */
class PowerSupplySensor : public PowerSupplySensorInherit
{
  public:
    /**
     * @brief Construct a new PowerSupplySensor object
     *
     * @param bus
     * @param path
     */
    PowerSupplySensor(sdbusplus::bus::bus& bus, const std::string& path) :
        PowerSupplySensorInherit(bus, path.c_str(),
                                 action::emit_interface_added)
    {}
};

/**
 * @brief Association for dbus
 */
class Association : public AssociationObject
{
  public:
    /**
     * @brief Construct a new Association object
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
 * @class PowerSupply
 */
class PowerSupply : public PowerSupplyInherit, public PSShellIntf
{
  public:
    PowerSupply() = delete;
    PowerSupply(const PowerSupply&) = delete;
    PowerSupply(PowerSupply&&) = delete;
    PowerSupply& operator=(const PowerSupply&) = delete;
    PowerSupply& operator=(PowerSupply&&) = delete;
    ~PowerSupply() = default;

    /**
     * @brief Construct a new Power Supply object
     *
     * @param bus
     * @param objPath
     * @param cmdUtilityName
     * @param name
     * @param assoc
     */
    PowerSupply(sdbusplus::bus::bus& bus, const std::string& objPath,
                const std::string& cmdUtilityName, const std::string& name,
                const std::string& assoc) :
        PowerSupplyInherit(bus, (objPath).c_str()),
        PSShellIntf(name, cmdUtilityName), bus(bus), inventoryPath(objPath),
        name(name)
    {
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            manufacturer(getManufacturer());
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            model(getModel());
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            partNumber(getPartNumber());
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            serialNumber(getSerialNumber());
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::present(true);
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::prettyName(
            "Power Supply " + name);
        sdbusplus::xyz::openbmc_project::State::Decorator::server::
            OperationalStatus::functional(true);
        sdbusplus::xyz::openbmc_project::State::Decorator::server::PowerState::
            powerState(sdbusplus::xyz::openbmc_project::State::Decorator::
                           server::PowerState::State::On);
        registerAssociationInterface(bus, objPath);
        registerSoftwareVersion(bus, objPath);
        registerSensorObject(bus, objPath);

        createAssociation(assoc);
    }

    void registerAssociationInterface(sdbusplus::bus::bus& bus,
                                      const std::string& ifPath)
    {
        AssociationList fwAssociation;
        std::string swpath = SW_INV_PATH;
        swpath += "/" + std::filesystem::path(ifPath).filename().string();
        fwAssociation.emplace_back(
            std::make_tuple("inventory", "activation", swpath));
        fwAssociation.emplace_back(std::make_tuple(
            upFwdAssociation, upRevAssociation, softwareUpdateablePath));

        AssociationObj =
            std::make_unique<Association>(bus, swpath, fwAssociation);
    }

    void registerSensorObject(sdbusplus::bus::bus& bus,
                              const std::string& ifPath)
    {
        std::string swpath = PSU_SENSOR_PATH;
        swpath += "/" + std::filesystem::path(ifPath).filename().string();

        PowerSupplySensorObj =
            std::make_unique<PowerSupplySensor>(bus, swpath.c_str());
        PowerSupplySensorObj->prettyName("Power Supply " + name);
        PowerSupplySensorObj->present(true);
        PowerSupplySensorObj->functional(true);
        PowerSupplySensorObj->powerState(
            sdbusplus::xyz::openbmc_project::State::Decorator::server::
                PowerState::State::On);
    }

    void registerSoftwareVersion(sdbusplus::bus::bus& bus,
                                 const std::string& ifPath)
    {
        std::string swpath = SW_INV_PATH;
        swpath += "/" + std::filesystem::path(ifPath).filename().string();
        std::string psuVersion = getVersion();
        VersionObj =
            std::make_unique<VersionInterface>(bus, swpath, psuVersion);
    }

    void updatePresence(bool present)
    {
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::present(
            present);
        PowerSupplySensorObj->present(present);
    }

    /**
     * @brief Get the Inventory Path object
     *
     * @return const std::string&
     */
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
        assocs.emplace_back(
            std::make_tuple("chassis", "inventory", reversePath));
        associations(assocs);
    }

  private:
    /** @brief systemd bus member */
    sdbusplus::bus::bus& bus;
    /**
     * @brief inventory Path
     *
     */
    std::string inventoryPath;
    std::string name;
    std::unique_ptr<VersionInterface> VersionObj;
    std::unique_ptr<Association> AssociationObj;
    std::unique_ptr<PowerSupplySensor> PowerSupplySensorObj;
    const std::string upFwdAssociation = "software_version";
    const std::string upRevAssociation = "updateable";
    const std::string purpose =
        "xyz.openbmc_project.Software.Version.VersionPurpose.Other";
    const std::string softwareUpdateablePath = "/xyz/openbmc_project/software";
};

} // namespace nvidia::power::psu
