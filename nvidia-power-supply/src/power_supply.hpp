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
        PSShellIntf(name, cmdUtilityName), bus(bus), inventoryPath(objPath)
    {
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            manufacturer(getManufacturer());
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            model(getModel());
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            partNumber(getPartNumber());
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset::
            serialNumber(getSerialNumber());
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::present(
            true);
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::prettyName(
            "Power Supply " + name);
        sdbusplus::xyz::openbmc_project::State::Decorator::server::
            OperationalStatus::functional(true);
        sdbusplus::xyz::openbmc_project::State::Decorator::server::PowerState::powerState(
            sdbusplus::xyz::openbmc_project::State::Decorator::server::PowerState::State::On
        );
        registerAssociationInterface(bus, objPath);
        registerSoftwareVersion(bus, objPath);

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
        sdbusplus::xyz::openbmc_project::Inventory::server::Item::present(present);
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
    std::unique_ptr<VersionInterface> VersionObj;
    std::unique_ptr<Association> AssociationObj;
    const std::string upFwdAssociation = "software_version";
    const std::string upRevAssociation = "updateable";
    const std::string purpose =
        "xyz.openbmc_project.Software.Version.VersionPurpose.Other";
    const std::string softwareUpdateablePath = "/xyz/openbmc_project/software";
};

} // namespace nvidia::power::psu
