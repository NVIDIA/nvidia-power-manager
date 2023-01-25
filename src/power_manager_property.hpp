#pragma once

#include <iostream>
#include <string>
#include <tuple>

#include "power_util.hpp"
using namespace nvidia::power::util;

using namespace phosphor::logging;

namespace nvidia::power::manager {

struct PowerCappingInfo {
  uint8_t revision{0x01};
  uint8_t mode{0x01};
  uint32_t currentPowerLimit{6500};
  uint32_t chassisPowerLimit_P{6500};
  uint32_t chassisPowerLimit_Q{5500};
  uint32_t chassisPowerLimit_Min{4900};
  uint32_t chassisPowerLimit_Max{6500};
  uint32_t restOfSystemPower{3300};
  uint8_t reserved[5]{};
  uint8_t checksum{0};
} __attribute__((packed));

struct NotSupportedInCurrentMode final
    : public sdbusplus::exception::generated_exception {
  static constexpr auto errName =
      "xyz.openbmc_project.Control.Power.Cap.Error.NotSupportedInCurrentMode";
  static constexpr auto errDesc =
      "Current Chassis Limit value can be updated only in "
      "xyz.openbmc_project.Control.Power.Mode.PowerMode.OEM Mode";
  static constexpr auto errWhat =
      "xyz.openbmc_project.Control.Power.Cap.Error.NotSupportedInCurrentMode: "
      "Current Chassis Limit value can be updated only in "
      "xyz.openbmc_project.Control.Power.Mode.PowerMode.OEM Mode";

  const char *name() const noexcept override;
  const char *description() const noexcept override;
  const char *what() const noexcept override;
};

struct ChassisLimitOutOfRange final
    : public sdbusplus::exception::generated_exception {
  static constexpr auto errName =
      "xyz.openbmc_project.Control.Power.Cap.Error.chassisLimitOutOfRange";
  static constexpr auto errDesc =
      " PowerCap (Current Chassis Limit) value should be between "
      "MaxPowerCapValue (chassisPowerLimit_Max) and MinPowerCapValue "
      "(chassisPowerLimit_Min)";
  static constexpr auto errWhat =
      "xyz.openbmc_project.Control.Power.Cap.Error.ChassisLimitOutOfRange: "
      "PowerCap (Current Chassis Limit) value should be between "
      "MaxPowerCapValue (chassisPowerLimit_Max) and MinPowerCapValue "
      "(chassisPowerLimit_Min)";

  const char *name() const noexcept override;
  const char *description() const noexcept override;
  const char *what() const noexcept override;
};

namespace property {

/**
 * @class property
 *
 * This class will create an object used to register power capping properties
 * supply devices.
 */

enum PowerMode {
  Static,
  PowerSaving,
  MaximumPerformance,
  OEM,
  Invalid = -1,
};
NLOHMANN_JSON_SERIALIZE_ENUM(
    PowerMode,
    {
        {Invalid, "xyz.openbmc_project.Control.Power.Mode.PowerMode.Invalid"},
        {Static, "xyz.openbmc_project.Control.Power.Mode.PowerMode.Static"},
        {PowerSaving,
         "xyz.openbmc_project.Control.Power.Mode.PowerMode.PowerSaving"},
        {MaximumPerformance,
         "xyz.openbmc_project.Control.Power.Mode.PowerMode.MaximumPerformance"},
        {OEM, "xyz.openbmc_project.Control.Power.Mode.PowerMode.OEM"},
    });

class Property {
public:
  Property() = delete;
  ~Property() = default;
  Property(const Property &) = delete;
  Property &operator=(const Property &) = delete;
  Property(Property &&) = delete;
  Property &operator=(Property &&) = delete;

  /**
   *
   *
   * @param[in] bus - D-Bus bus object
   * @param[in] enabledInterface - interface object
   * @param[in] Jsondata - interface object
   * @param[in] powerCappingInfo - power capping structure
   */
  Property(sdbusplus::bus::bus &bus,
           std::shared_ptr<sdbusplus::asio::dbus_interface> enabledInterface,
           nlohmann::json Jsondata, PowerCappingInfo &powerCappingInfo,
           std::string module)
      : Bus(bus), iface(enabledInterface), propJson(Jsondata),
        powerCapInfo(powerCappingInfo), powerModule(module) {
    propertyname = propJson["propertyName"];
    if (propertyname == "PowerCap") {
      if (powerModule == "System") {
        if (static_cast<PowerMode>(powerCapInfo.mode) == OEM) {
          _value = powerCapInfo.currentPowerLimit;
        } else if (static_cast<PowerMode>(powerCapInfo.mode) == PowerSaving) {
          _value = powerCapInfo.chassisPowerLimit_Q;
        } else if (static_cast<PowerMode>(powerCapInfo.mode) ==
                   MaximumPerformance) {
          _value = powerCapInfo.chassisPowerLimit_P;
        }
      } else {
        _value = propJson["value"];
      }
    } else if (propertyname == "MinPowerCapValue") {
      _value = powerCapInfo.chassisPowerLimit_Min;
    } else if (propertyname == "MaxPowerCapValue" &&
               iface->get_object_path() ==
                   "/xyz/openbmc_project/control/power/CurrentChassisLimit") {
      _value = powerCapInfo.chassisPowerLimit_Max;
    } else if (propertyname == "PowerMode") {
      _mode = static_cast<PowerMode>(powerCapInfo.mode);

    } else if (propertyname == "MaxPowerCapValue" &&
               iface->get_object_path() ==
                   "/xyz/openbmc_project/control/power/ChassisLimitQ") {
      _value = powerCapInfo.chassisPowerLimit_Q;
    } else if (propertyname == "MaxPowerCapValue" &&
               iface->get_object_path() ==
                   "/xyz/openbmc_project/control/power/ChassisLimitP") {
      _value = powerCapInfo.chassisPowerLimit_P;
    } else if (propertyname == "Value") {
      _value = powerCapInfo.restOfSystemPower;
    }

    if (propertyname == "PowerMode") {
      if ("sdbusplus::asio::PropertyPermission::readWrite" ==
          propJson["flags"]) {
        iface->register_property(
            propertyname, convertPowerModeToString(_mode),
            [this](const auto &newPropertyValue, const auto &) {
              auto mode = convertStringToPowerMode(newPropertyValue);
              if (mode == Invalid) {
                throw sdbusplus::exception::InvalidEnumString();

              } else {
                if (_mode != mode) {
                  _mode = mode;
                  iface->signal_property(propertyname);
                  powerCapInfo.mode = _mode;
                }
              }

              return 0;
            },
            [this](const auto &) { return convertPowerModeToString(_mode); });
      } else {
        iface->register_property(propertyname, convertPowerModeToString(_mode),
                                 sdbusplus::asio::PropertyPermission::readOnly);
      }
    } else {
      if ("sdbusplus::asio::PropertyPermission::readWrite" ==
          propJson["flags"]) {
        iface->register_property(
            propertyname, _value,
            [this](const auto &newPropertyValue, const auto &) {
                if (_value != newPropertyValue) {
                  if (newPropertyValue <= powerCapInfo.chassisPowerLimit_Max &&
                      newPropertyValue >= powerCapInfo.chassisPowerLimit_Min) {
                    _value = newPropertyValue;
                    powerCapInfo.currentPowerLimit = _value;
                    iface->signal_property(propertyname);
                    if (static_cast<PowerMode>(powerCapInfo.mode) != OEM) {
                      powerCapInfo.mode = static_cast<int>(OEM);
                    }
                  } else {
                    std::cerr
                        << "Current Chassis Limit value should be between "
                        << powerCapInfo.chassisPowerLimit_Max << "-"
                        << powerCapInfo.chassisPowerLimit_Min << std::endl;
                    throw ChassisLimitOutOfRange();
                  }
                }
              return 0;
            },
            [this](const auto &) { return _value; });
      } else {
        iface->register_property_r(propertyname, _value,
                                   sdbusplus::vtable::property_::emits_change,
                                   [this](const auto &) { return _value; });
      }
    }
  }

  /** @brief Convert an string value to a enum.
   *  @param[in] s - The string to convert to a enum.
   *  @return - PowerMode enum value corresponding to
   *            "xyz.openbmc_project.Control.Power.Mode.<value name>"
   */

  static PowerMode convertStringToPowerMode(const std::string &s) noexcept {
    nlohmann::json j = s.c_str();

    return j.get<PowerMode>();
  }

  /** @brief Convert an enum value to a string.
   *  @param[in] e - The enum to convert to a string.
   *  @return - The string conversion in the form of
   *            "xyz.openbmc_project.Control.Power.Mode.<value name>"
   */
  std::string convertPowerModeToString(PowerMode v) {
    nlohmann::json j = v;

    return j;
  }

  /** @brief method to update PowerCap value.
   *  @param[in] value - the new input value .
   * @param[in] emitsChange - emits change signal boolean value .
   */
  void updateValue(uint32_t value, bool emitsChange) {
    if (_value != value) {
      _value = value;
      if (propertyname == "PowerCap" && powerModule == "System") {
        powerCapInfo.currentPowerLimit = _value;
      }
      if (emitsChange) {
        iface->signal_property(propertyname);
      }
    }
  }

  void updateMode(uint8_t mode, bool emitsChange) {
    if (static_cast<PowerMode>(mode) == Invalid) {
      throw sdbusplus::exception::InvalidEnumString();

    } else {
      if (_mode != static_cast<PowerMode>(mode)) {
        _mode = static_cast<PowerMode>(mode);
        if (emitsChange) {
          iface->signal_property(propertyname);
        }
        powerCapInfo.mode = _mode;
      }
    }
  }
  std::string getPropertyName() { return propertyname; }

  std::string getPowerModuleName() { return powerModule; }

  std::string getPowerMode() { return convertPowerModeToString(_mode); }

  uint32_t getValue() { return _value; }

  std::shared_ptr<sdbusplus::asio::dbus_interface> getInterfaceName() {
    return iface;
  }

  void triggerEmitChangeSignal() { iface->signal_property(propertyname); }

private:
  /**
   * The D-Bus object
   */
  sdbusplus::bus::bus &Bus;
  std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

  nlohmann::json propJson;
  PowerCappingInfo &powerCapInfo;
  uint32_t _value = 0;
  PowerMode _mode;
  std::string propertyname;
  std::string powerModule;
};

} // namespace property

} // namespace nvidia::power::manager