#pragma once

#include "cpld_util.hpp"

#include <filesystem>
#include <iostream>
#include <sdbusplus/bus/match.hpp>
#include <stdexcept>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
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
         const std::string& manufacturerN) :
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
        auto chassisType =
            sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis::
                convertChassisTypeFromString("xyz.openbmc_project.Inventory."
                                             "Item.Chassis.ChassisType.Module");
        sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis::type(
            chassisType);
    }

    const std::string& getInventoryPath() const
    {
        return inventoryPath;
    }

  private:
    /** @brief systemd bus member */
    sdbusplus::bus::bus& bus;
    std::string inventoryPath;
};

} // namespace nvidia::cpld::device
