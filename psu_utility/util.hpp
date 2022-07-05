#pragma once

#include <fmt/format.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace LoggingServer = sdbusplus::xyz::openbmc_project::Logging::server;
using Level = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

using json = nlohmann::json;
using namespace phosphor::logging;
const std::string trargetDetermined{"Update.1.0.TargetDetermined"};
const std::string applyFailed{"Update.1.0.ApplyFailed"};
const std::string updateSuccessful{"Update.1.0.UpdateSuccessful"};
const std::string transferFailed{"Update.1.0.TransferFailed"};
const std::string transferringToComponent{"Update.1.0.TransferringToComponent"};
const std::string awaitToActivate{"Update.1.0.AwaitToActivate"};

/**
 * @brief Loads json files
 *
 * @param path
 * @return json
 */
inline json loadJSONFile(const char *path) {
  std::ifstream ifs(path);

  if (!ifs.good()) {
    log<level::ERR>(std::string("Unable to open file "
                                "PATH=" +
                                std::string(path))
                        .c_str());
    return nullptr;
  }
  auto data = json::parse(ifs, nullptr, false);
  if (data.is_discarded()) {
    log<level::ERR>(std::string("Failed to parse json "
                                "PATH=" +
                                std::string(path))
                        .c_str());
    return nullptr;
  }
  return data;
}

inline void createLog(const std::string &messageID,
                      std::map<std::string, std::string> &addData,
                      Level &level) {
  static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
  static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";
  static constexpr auto service = "xyz.openbmc_project.Logging";
  try {
    auto bus = sdbusplus::bus::new_default();
    auto severity = LoggingServer::convertForMessage(level);
    auto method =
        bus.new_method_call(service, logObjPath, logInterface, "Create");
    method.append(messageID, severity, addData);
    bus.call_noreply(method);
  } catch (const std::exception &e) {
    std::cerr << "Failed to create D-Bus log entry for message registry, ERROR="
              << e.what() << "\n";
  }
}
