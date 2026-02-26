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

#include "gpuCpuPowerSync.hpp"
#include "utils.hpp"

#include <boost/asio/io_context.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>
constexpr auto BUSNAME = "com.Nvidia.PowerBalancer";

int main(void)
{
    try
    {
        boost::asio::io_context io;
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

        systemBus->request_name(BUSNAME);
        [[maybe_unused]] sdbusplus::asio::object_server objectServer(systemBus);
        nvidia::power::balancer::GpuCpuPowerSync gpuCpuPowerSync(systemBus);
        lg2::info("nvidia-power-balancerd started");
        io.run();
        lg2::info("nvidia-power-balancerd stopped");
        return 0;
    }
    catch (const std::exception& e)
    {
        lg2::error(e.what());
        return -EXIT_FAILURE;
    }
    catch (...)
    {
        lg2::error("Caught unexpected exception type");
        return -EXIT_FAILURE;
    }
}
