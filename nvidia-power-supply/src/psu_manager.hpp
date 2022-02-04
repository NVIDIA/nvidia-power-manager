#pragma once

#include "config.h"

#include "power_supply.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

using namespace nvidia::power::psu;
using namespace phosphor::logging;
using json = nlohmann::json;

namespace nvidia::power::manager
{

/**
 * @brief Power supply manager is collection of Power supplies
 *        manages the power supplies
 *
 */
class PSUManager
{
  public:
    PSUManager() = delete;
    ~PSUManager() = default;
    PSUManager(const PSUManager&) = delete;
    PSUManager& operator=(const PSUManager&) = delete;
    PSUManager(PSUManager&&) = delete;
    PSUManager& operator=(PSUManager&&) = delete;

    /**
     * @brief Construct a new PSUManager object
     *
     * @param bus
     * @param e
     */
    PSUManager(sdbusplus::bus::bus& bus, std::string baseInvPath);

  private:
    /**
     * @brief d-Bus
     *
     */
    sdbusplus::bus::bus& bus;

    /**
     * @brief collection of PSUs
     *
     */
    std::vector<std::unique_ptr<PowerSupply>> psus;

    /**
     * @brief Base path for PSU inventory
     * 
     */
    std::string baseInventoryPath;
};

} // namespace nvidia::power::manager
