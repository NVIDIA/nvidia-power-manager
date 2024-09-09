/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
