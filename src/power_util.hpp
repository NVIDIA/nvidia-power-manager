#pragma once

#include "config.h"
#include <boost/asio/io_service.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <regex>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/vtable.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace nvidia {
namespace power {
namespace util {

using namespace phosphor::logging;
using json = nlohmann::json;
constexpr auto PROPERTY_INTF = "org.freedesktop.DBus.Properties";

/**
 * @brief Get the service name from the mapper for the
 *        interface and path passed in.
 *
 * @param[in] path - the D-Bus path name
 * @param[in] interface - the D-Bus interface name
 * @param[in] bus - the D-Bus object
 * @param[in] logError - log error when no service found
 *
 * @return The service name
 */
std::string getService(const std::string &path, const std::string &interface,
                       sdbusplus::bus::bus &bus, bool logError = true);

std::vector<std::string>
getSubtreePaths(sdbusplus::bus::bus &bus,
                const std::vector<std::string> &interfaces,
                const std::string &path);

std::map<std::string, std::vector<std::string>>
getInterfaces(const std::string &path, sdbusplus::bus::bus &bus);

/**
 * @brief Read a D-Bus property
 *
 * @param[in] interface - the interface the property is on
 * @param[in] propertName - the name of the property
 * @param[in] path - the D-Bus path
 * @param[in] service - the D-Bus service
 * @param[in] bus - the D-Bus object
 * @param[out] value - filled in with the property value
 */
template <typename T>
void getProperty(const std::string &interface, const std::string &propertyName,
                 const std::string &path, const std::string &service,
                 sdbusplus::bus::bus &bus, T &value) {
  std::variant<T> property;

  auto method =
      bus.new_method_call(service.c_str(), path.c_str(), PROPERTY_INTF, "Get");

  method.append(interface, propertyName);

  auto reply = bus.call(method);

  reply.read(property);
  value = std::get<T>(property);
}

/**
 * @brief Write a D-Bus property
 *
 * @param[in] interface - the interface the property is on
 * @param[in] propertName - the name of the property
 * @param[in] path - the D-Bus path
 * @param[in] service - the D-Bus service
 * @param[in] bus - the D-Bus object
 * @param[in] value - the value to set the property to
 */
template <typename T>
void setProperty(const std::string &interface, const std::string &propertyName,
                 const std::string &path, const std::string &service,
                 sdbusplus::bus::bus &bus, T &value) {
  std::variant<T> propertyValue(value);

  auto method =
      bus.new_method_call(service.c_str(), path.c_str(), PROPERTY_INTF, "Set");

  method.append(interface, propertyName, propertyValue);

  auto reply = bus.call(method);
}

/**
 * @brief load a json Config file
 *
 * @param[in] path - the absolute path for json configuration file
 *
 */
json loadJSONFromFile(const char *path);

/**
 * @brief load a power cap binary file
 *
 * @param[in] path - the absolute path for power cap binary file
 *
 */
uint8_t *loadPowerCapInfoFromFile(const char *path);
/**
 * @brief dump a power cap structure to file
 *
 * @param[in] path - the absolute path for power cap binary file
 * @param[in] data - datat buffer to be dumped
 * @param[in] length - length of the data buffer
 *
 */
int dumpPowerCapIntoFile(const char *path, uint8_t *data, int length);

} // namespace util
} // namespace power
} // namespace nvidia