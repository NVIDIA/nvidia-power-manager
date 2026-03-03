/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
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

#pragma once

#include "jobMonitor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <memory>
#include <unordered_map>
#define AssociationInterface "xyz.openbmc_project.Association"
#define ObjectMapperService "xyz.openbmc_project.ObjectMapper"
#define EndpointProperty "endpoints"
#define EntityManagerService "xyz.openbmc_project.EntityManager"
#define CpuInterface "xyz.openbmc_project.Inventory.Item.Cpu"
#define GpuInterface "xyz.openbmc_project.Inventory.Item.Accelerator"
#define LocationContextInterface                                               \
    "xyz.openbmc_project.Inventory.Decorator.LocationContext"
#define PowerCapInterface "xyz.openbmc_project.Control.Power.Cap"
#define PowerCapProperty "PowerCap"
#define DefaultPowerCap 0
#define PowerCapInvalid 0xFFFFFFFF

namespace nvidia::power::balancer
{

enum class DeviceType
{
    GPU,
    CPU
};

using GPUDeviceName = std::string;

struct DeviceInfo
{
    std::string serviceName;
    std::string objectPath;
    std::string interfaceName;
    std::string propertyName;
    uint32_t powerCapValue = DefaultPowerCap;
    std::shared_ptr<JobMonitor> jobMonitor = nullptr;
    std::unique_ptr<sdbusplus::bus::match_t> powerCapChangedSignal;
    std::unique_ptr<sdbusplus::bus::match_t> powerCapInterfaceAddedSignal;
};

struct moduleDeviceInfo
{
    DeviceInfo cpuInfo;
    std::unordered_map<GPUDeviceName, DeviceInfo> connectedGpuInfos;
};

class GpuCpuPowerSync
{
  public:
    GpuCpuPowerSync(std::shared_ptr<sdbusplus::asio::connection> bus);
    ~GpuCpuPowerSync() = default;
    void run();

  protected:
    // Testable methods - exposed as protected for unit testing
    std::string extractServiceName(
        DeviceType type, const std::map<std::string, std::vector<std::string>>&
                             servicesAndInterfaces);

    void updateDeviceInfo(DeviceInfo& deviceInfo, const std::string& objectPath,
                          const std::string& serviceName,
                          const std::string& interfaceName,
                          const std::string& propertyName,
                          uint32_t powerCapValue);

    void processAssociationEndpoints(DeviceType type,
                                     const std::string& deviceName,
                                     const std::string& locationContext,
                                     const std::vector<std::string>& endpoints);

    void syncPowerCapForAllGpus(const std::string& locationContext);

    void onServiceDiscovered(
        DeviceType type, const std::string& deviceName,
        const std::string& locationContext, const std::string& powerLimitPath,
        boost::system::error_code ec,
        const std::map<std::string, std::vector<std::string>>&
            servicesAndInterfaces);

    void onPowerCapRetrieved(DeviceType type, const std::string& deviceName,
                             const std::string& locationContext,
                             const std::string& powerLimitPath,
                             const std::string& serviceName,
                             boost::system::error_code ec, uint32_t powerCap);

    void onLocationContextFetched(DeviceType type,
                                  const std::string& objectPath,
                                  boost::system::error_code ec,
                                  std::string locationContext);

    void onDeviceTypeResolved(
        const std::string& objectPath, const std::string& locationContext,
        boost::system::error_code ec,
        const std::map<std::string, std::vector<std::string>>&
            servicesAndInterfaces);

    // Testable state
    std::map<std::string, moduleDeviceInfo> platformCpuGpuMap;

  private:
    std::shared_ptr<sdbusplus::asio::connection> bus_;
    sdbusplus::bus::match_t emDeviceAddedSignal;

    void emDeviceAddedHandler(sdbusplus::message::message& msg);
    void discoverGpuDevice(const std::string& deviceName,
                           const std::string& locationContext,
                           const std::string& cpuPowerLimitPath);
    void discoverCpuDevice(const std::string& deviceName,
                           const std::string& locationContext,
                           const std::string& cpuPowerLimitPath);
    void associationsInterfaceAddedHandler(DeviceType type,
                                           const std::string& deviceName,
                                           const std::string& locationContext,
                                           const std::string& associationPath,
                                           sdbusplus::message::message& msg);
    void associationsPropertyChangedHandler(DeviceType type,
                                            const std::string& deviceName,
                                            const std::string& locationContext,
                                            sdbusplus::message::message& msg);
    void powerCapChangedHandler(DeviceType type, const std::string& deviceName,
                                const std::string& locationContext,
                                sdbusplus::message::message& msg);
    void powerCapInterfaceAddedHandler(DeviceType type,
                                       const std::string& deviceName,
                                       const std::string& locationContext,
                                       const std::string& cpuPowerLimitPath,
                                       sdbusplus::message::message& msg);
    void discoverGpuViaCpuPowerAssociation(const std::string& objectPath,
                                           const std::string& locationContext);
    void
        discoverCpuViaPowerLimitAssociation(const std::string& objectPath,
                                            const std::string& locationContext);
    void setPowerCapOnGpu(const std::string& deviceName,
                          const std::string& locationContext);
    void registerPowerCapSignalHandlers(DeviceType type, DeviceInfo& deviceInfo,
                                        const std::string& deviceName,
                                        const std::string& locationContext,
                                        const std::string& powerLimitPath);

    void fetchLocationContextAndDiscover(DeviceType type,
                                         const std::string& objectPath);

    void resolveDeviceTypeAndDiscover(const std::string& objectPath,
                                      const std::string& locationContext);

    std::unordered_map<std::string, sdbusplus::bus::match_t>
        associationsInterfaceAddedSignals;
    std::unordered_map<std::string, sdbusplus::bus::match_t>
        associationsPropertyChangedSignals;
};
} // namespace nvidia::power::balancer
