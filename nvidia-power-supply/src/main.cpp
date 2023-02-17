
#include "psu_manager.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdeventplus/event.hpp>

#include <filesystem>

using namespace nvidia::power;

int main(void)
{
    try
    {
        using namespace phosphor::logging;

        auto bus = sdbusplus::bus::new_default();
        auto event = sdeventplus::Event::get_default();

        sdbusplus::server::manager::manager objManager(bus, BASE_INV_PATH);

        bus.request_name(BUSNAME);
        bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

        manager::PSUManager manager(bus, BASE_INV_PATH);

        return event.loop();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(e.what());
        return -1;
    }
}
