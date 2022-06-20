#pragma once

#include <sdbusplus/asio/object_server.hpp>

namespace nvidia::psumonitor::event {
/**
 * @class PsuEvent
 */
class PsuEvent {
public:
  /**
   * @brief Construct a new PsuEvent object
   *
   * @param name
   * @param objectserver
   * @param event
   */
  PsuEvent(const std::string &name,
           sdbusplus::asio::object_server &objectServer);
  ~PsuEvent();

  /**
   * @brief dbus oject server
   *
   */
  sdbusplus::asio::object_server &objServer;
  /**
   * @brief d-Bus interface
   *
   */
  std::shared_ptr<sdbusplus::asio::dbus_interface> enabledInterface;
  /**
   * @brief psu event name
   *
   */
  std::string name;
};

} // namespace nvidia::psumonitor::event
