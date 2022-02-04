
#include "cpld_manager.hpp"

#include <filesystem>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdeventplus/event.hpp>

using namespace nvidia::cpld;

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

        manager::CPLDManager manager(bus, BASE_INV_PATH);

        return event.loop();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(e.what());
        return -2;
    }
    catch (...)
    {
        log<level::ERR>("Caught unexpected exception type");
        return -3;
    }
}
