#include "PsuEvent.hpp"

#include <iostream>
#include <string>
#include <unistd.h>

namespace nvidia::psumonitor::event {
/**
 * @class PsuEvent
 */
PsuEvent::PsuEvent(const std::string &name,
                   sdbusplus::asio::object_server &objectServer)
    : objServer(objectServer), name(std::move(name)) {

  enabledInterface =
      objServer.add_interface("/xyz/openbmc_project/sensors/power/" + name,
                              "xyz.openbmc_project.Object.Enable");

  enabledInterface->register_property(
      "Enabled", true, sdbusplus::asio::PropertyPermission::readWrite);

  if (!enabledInterface->initialize()) {
    std::cerr << "error initializing enabled interface\n";
  }
}

PsuEvent::~PsuEvent() { objServer.remove_interface(enabledInterface); }

} // namespace nvidia::psumonitor::event
