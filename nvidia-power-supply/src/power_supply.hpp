#pragma once

#include "psu_util.hpp"

#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PowerSupply/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
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
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using AssociationObject = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using AssociationIfaceMap =
    std::map<std::string, std::unique_ptr<AssociationObject>>;

using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

using PropertyType = std::variant<std::string, bool>;

using Properties = std::map<std::string, PropertyType>;

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

        createAssociation(assoc);
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
};

} // namespace nvidia::power::psu
