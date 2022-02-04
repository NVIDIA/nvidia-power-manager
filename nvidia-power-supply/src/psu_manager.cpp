#include "psu_manager.hpp"

#include <fmt/format.h>
#include <sys/types.h>
#include <unistd.h>

#include <xyz/openbmc_project/Common/Device/error.hpp>

#include <fstream>
#include <iostream>
#include <regex>

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Device::Error;
using namespace nvidia::power::common;

namespace nvidia::power::manager
{

PSUManager::PSUManager(sdbusplus::bus::bus& bus, std::string baseInvPath) :
    bus(bus), baseInventoryPath(baseInvPath)
{

    using namespace sdeventplus;

    nlohmann::json fruJson = loadJSONFile(PSU_JSON_PATH);
    if (fruJson == nullptr)
    {
        log<level::ERR>("InternalFailure when parsing the JSON file");
        return;
    }
    std::string cmdUtilityName = "psui2ccmd.sh";
    try
    {
        cmdUtilityName = fruJson.at("CommandName");
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    for (const auto& fru : fruJson.at("PowerSupplies"))
    {
        try
        {
            std::string id = fru.at("Index");
            std::string assoc = fru.at("ChassisAssociationEndpoint");
            std::string invpath = baseInventoryPath + "/powersupply" + id;
            auto invMatch =
                std::find_if(psus.begin(), psus.end(), [&invpath](auto& psu) {
                    return psu->getInventoryPath() == invpath;
                });
            if (invMatch != psus.end())
            {
                continue;
            }

            auto psu = std::make_unique<PowerSupply>(bus, invpath, cmdUtilityName, id, assoc);
            psus.emplace_back(std::move(psu));
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

} // namespace nvidia::power::manager
