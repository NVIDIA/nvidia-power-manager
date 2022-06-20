#include "PsuEvent.hpp"
#include "PsuMonitor.hpp"

#include <boost/asio/io_service.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace nvidia::psumonitor;
using namespace nvidia::psumonitor::event;

int main(void) {

  int psuEve[3]{1, 2, 3};
  std::vector<std::shared_ptr<PsuEvent>> psuEvents;

  std::string eventRegisters[3] = {"psu_drop_to_1_event", "psu_drop_to_2_event",
                                   "MBON_GBOFF_event"};

  try {

    boost::asio::io_service io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    systemBus->request_name("com.Nvidia.PsuEvent");
    sdbusplus::asio::object_server objectServer(systemBus);

    for (const auto &i : psuEve) {
      auto psuEvent =
          std::make_unique<PsuEvent>(eventRegisters[i - 1], objectServer);

      psuEvents.emplace_back(std::move(psuEvent));
    }

    PsuMonitor psuMonitor(io, psuEvents);
    psuMonitor.start();

    io.run();

  }

  catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
