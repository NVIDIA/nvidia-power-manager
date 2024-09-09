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

int main(void)
{
    int psuEve[3]{1, 2, 3};
    std::vector<std::shared_ptr<PsuEvent>> psuEvents;

    std::string eventRegisters[3] = {"psu_drop_to_1_event",
                                     "psu_drop_to_2_event", "MBON_GBOFF_event"};

    try
    {
        boost::asio::io_service io;
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

        systemBus->request_name("com.Nvidia.PsuEvent");
        sdbusplus::asio::object_server objectServer(systemBus);

        for (const auto& i : psuEve)
        {
            auto psuEvent = std::make_unique<PsuEvent>(eventRegisters[i - 1],
                                                       objectServer);

            psuEvents.emplace_back(std::move(psuEvent));
        }

        PsuMonitor psuMonitor(io, psuEvents);
        psuMonitor.start();

        io.run();
    }

    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
