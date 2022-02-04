#include "cpld_manager.hpp"

#include <fmt/format.h>
#include <sys/types.h>
#include <unistd.h>

#include <regex>
#include <xyz/openbmc_project/Common/Device/error.hpp>

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Device::Error;
using namespace nvidia::cpld::common;

namespace nvidia::cpld::manager
{

CPLDManager::CPLDManager(sdbusplus::bus::bus& bus, std::string basePath) :
    bus(bus)
{
    using namespace sdeventplus;

    nlohmann::json fruJson = loadJSONFile(CPLD_JSON_PATH);
    if (fruJson == nullptr)
    {
        log<level::ERR>("InternalFailure when parsing the JSON file");
        return;
    }
    for (const auto& fru : fruJson.at("CPLD"))
    {
        try
        {
            const auto baseinvInvPath = basePath + "/Cpld";

            std::string id = fru.at("Index");
            std::string busN = fru.at("Bus");
            std::string address = fru.at("Address");
            std::string invpath = baseinvInvPath + id;

            uint8_t busId = std::stoi(busN);
            uint8_t devAddr = std::stoi(address, nullptr, 16);

            auto invMatch = std::find_if(
                cpldInvs.begin(), cpldInvs.end(), [&invpath](auto& inv) {
                    return inv->getInventoryPath() == invpath;
                });
            if (invMatch != cpldInvs.end())
            {
                continue;
            }
            auto inv =
                std::make_unique<Cpld>(bus, invpath, busId, devAddr, id);
            cpldInvs.emplace_back(std::move(inv));
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

} // namespace nvidia::cpld::manager
