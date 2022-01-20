#pragma once

#include "config.h"

#include "cpld.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

using namespace nvidia::cpld::device;
using namespace phosphor::logging;
using json = nlohmann::json;

namespace nvidia::cpld::manager
{

class CPLDManager
{
  public:
    CPLDManager() = delete;
    ~CPLDManager() = default;
    CPLDManager(const CPLDManager&) = delete;
    CPLDManager& operator=(const CPLDManager&) = delete;
    CPLDManager(CPLDManager&&) = delete;
    CPLDManager& operator=(CPLDManager&&) = delete;

    CPLDManager(sdbusplus::bus::bus& bus, std::string basePath);

  private:
    sdbusplus::bus::bus& bus;

    std::vector<std::unique_ptr<Cpld>> cpldInvs;
};

} // namespace nvidia::cpld::manager
