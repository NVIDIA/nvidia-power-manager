#pragma once

#include "config.h"

#include "PsuEvent.hpp"
#include "utils.hpp"

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/State/Decorator/PowerState/server.hpp>

#include <memory>
#include <string>

using json = nlohmann::json;

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

using PowerState = sdbusplus::xyz::openbmc_project::State::Decorator::server::
    PowerState::State;

using namespace nvidia::psumontior::utils;
using namespace nvidia::psumonitor::event;

namespace nvidia::psumonitor {

static constexpr unsigned int intrusionSensorPollSec = 5;
static constexpr unsigned int eveOffset = 4;

// dbus properties
static constexpr auto present = "Present";
static constexpr auto functional = "Functional";
static constexpr auto powerState = "PowerState";
static constexpr auto enable = "Enabled";
// interfaces
static constexpr auto powerStateIface =
    "xyz.openbmc_project.State.Decorator.PowerState";
static constexpr auto operationalIface =
    "xyz.openbmc_project.State.Decorator.OperationalStatus";
static constexpr auto enableIface = "xyz.openbmc_project.Object.Enable";
static constexpr auto powerSupplyIface =
    "xyz.openbmc_project.Inventory.Item.PowerSupply";

/*
 * @class PowerMonitorSensor
 */
class PsuMonitor {
public:
  /**
   * @brief Construct a new Power Supply object
   *
   * @param bus
   * @param dbud service
   * @param psuevents container object
   */
  PsuMonitor(boost::asio::io_service &io,
             std::vector<std::shared_ptr<PsuEvent>> &pEvents);

  ~PsuMonitor();

  void start();

private:
  std::vector<std::string> psuObjectPaths;
  std::vector<std::string> psuEveObjectPaths;

  /** @brief psu events vector*/
  std::vector<std::shared_ptr<PsuEvent>> psuEvents;

  /** @brief timer  */
  boost::asio::deadline_timer mPollTimer;

  /** @brief psu i2c details */
  int bus;
  int slaveAddress;

  /** @brief i2c register address */
  int detectRegAddr;
  int alertRegAddr;
  int workRegAddr;
  int psuDropRegAddr;

  /** @brief i2c register values*/
  int32_t detectRegValue;
  int32_t alertRegValue;
  int32_t workRegValue;
  int32_t psuDropRegValue;

  /** DBusHandler class handles the D-Bus operations */
  DBusHandler dBusHandler;

  /**
   * @brief Loads json file and get psu configuration deatails
   * @return integer
   */
  int getPsuConfigValues();

  std::string getPowerStateEnumeration(bool value);

  /**
   * @brief update dbus property
   * @param dbus object path
   * @param dbus interface
   * @param dbus property name
   * @param proeprty value
   */
  void udpateProperty(const std::string &objectPath, const std::string &iface,
                      const std::string &propertyName, bool value);

  /**
   * @brief read i2c regsiter value
   * @param i2c bus number
   * @param slave address
   * @param register address
   * @return register value
   */
  int32_t i2cRead(const int &busId, const int &slaveAddr,
                  const int32_t &statusReg);

  /**
   * @brief update status based on cpld registers
   * @param regsiter values
   */
  int update_status(const int32_t &detectNewVal, const int32_t &alertNewVal,
                    const int32_t &workNewVal, const int32_t &dropNewVal,
                    int initialize);

  /**
   * @brief polling cpld regsiters
   * It will monitor the register changes
   * with an interval of 10 seconds.
   *
   */
  void pollRegisters();
};

} // namespace nvidia::psumonitor
