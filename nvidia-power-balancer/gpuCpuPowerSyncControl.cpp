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

#include "config.h"

#include "gpuCpuPowerSync.hpp"

namespace nvidia::power::balancer
{
void GpuCpuPowerSync::setPowerCapOnGpu(const std::string& deviceName,
                                       const std::string& locationContext)
{
    lg2::info(
        "setPowerCapOnGpu:: Request to set power cap on GPU: {DEVICE_NAME} on {LOCATION_CONTEXT}",
        "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext);

    auto& cpuInfo = platformCpuGpuMap[locationContext].cpuInfo;
    auto& gpuInfo =
        platformCpuGpuMap[locationContext].connectedGpuInfos[deviceName];

    // Check if devices are ready for power cap sync
    if (cpuInfo.objectPath.empty() ||
        cpuInfo.powerCapValue == DefaultPowerCap ||
        cpuInfo.powerCapValue == PowerCapInvalid)
    {
        lg2::error(
            "setPowerCapOnGpu:: CPU not ready for power cap sync on Location Context: {LOCATION_CONTEXT}, objectPath: {OBJECT_PATH}, powerCap: {POWER_CAP}",
            "LOCATION_CONTEXT", locationContext, "OBJECT_PATH",
            cpuInfo.objectPath, "POWER_CAP", cpuInfo.powerCapValue);
        return;
    }

    if (gpuInfo.objectPath.empty() || gpuInfo.serviceName.empty() ||
        gpuInfo.powerCapValue == PowerCapInvalid)
    {
        lg2::error(
            "setPowerCapOnGpu:: GPU {DEVICE_NAME} not ready for power cap sync on Location Context: {LOCATION_CONTEXT}, objectPath: {OBJECT_PATH}, serviceName: {SERVICE_NAME}, powerCap: {POWER_CAP}",
            "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
            "OBJECT_PATH", gpuInfo.objectPath, "SERVICE_NAME",
            gpuInfo.serviceName, "POWER_CAP", gpuInfo.powerCapValue);
        return;
    }

    // Skip if already in sync
    if (cpuInfo.powerCapValue == gpuInfo.powerCapValue)
    {
        lg2::info(
            "Power cap already in sync for GPU: {DEVICE_NAME} on {LOCATION_CONTEXT}, CPU_Power_Cap value is {POWER_CAP} and GPU_Power_Cap value is {GPU_POWER_CAP}",
            "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
            "POWER_CAP", cpuInfo.powerCapValue, "GPU_POWER_CAP",
            gpuInfo.powerCapValue);
        return;
    }

    utils::setPropertyAsyncNvidia<uint32_t>(
        bus_, gpuInfo.serviceName, gpuInfo.objectPath, gpuInfo.interfaceName,
        gpuInfo.propertyName, cpuInfo.powerCapValue,
        [this, deviceName, locationContext](boost::system::error_code ec,
                                            std::string jobPath) {
        if (ec)
        {
            lg2::error(
                "Failed to set power cap on GPU: {DEVICE_NAME} on {LOCATION_CONTEXT}: {ERROR}",
                "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
                "ERROR", ec.message());
            return;
        }

        auto& cpuInfo = platformCpuGpuMap[locationContext].cpuInfo;
        auto& gpuInfo =
            platformCpuGpuMap[locationContext].connectedGpuInfos[deviceName];

        lg2::info(
            "Tried Power cap patch on Device: {DEVICE_NAME} on Location Context: {LOCATION_CONTEXT} with value : {POWER_CAP}  and job path: {JOB}",
            "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
            "POWER_CAP", cpuInfo.powerCapValue, "JOB", jobPath);

        gpuInfo.jobMonitor = JobMonitor::create(
            bus_, gpuInfo.serviceName, jobPath, deviceName, locationContext,
            cpuInfo.powerCapValue,
            std::chrono::seconds(JOB_MONITOR_TIMEOUT_SECONDS), nullptr);
    });
}

void GpuCpuPowerSync::syncPowerCapForAllGpus(const std::string& locationContext)
{
    for (const auto& [gpuDeviceName, _] :
         platformCpuGpuMap[locationContext].connectedGpuInfos)
    {
        setPowerCapOnGpu(gpuDeviceName, locationContext);
    }
}

void GpuCpuPowerSync::powerCapChangedHandler(DeviceType type,
                                             const std::string& deviceName,
                                             const std::string& locationContext,
                                             sdbusplus::message::message& msg)
{
    std::string objectPath = msg.get_path();
    std::string interface;
    utils::PropertyMap properties;
    msg.read(interface, properties);

    for (const auto& [property, value] : properties)
    {
        if (property == PowerCapProperty)
        {
            uint32_t powerCap = std::get<uint32_t>(value);
            lg2::info(
                "Power cap changed Device: {DEVICE_NAME} on Location Context: {LOCATION_CONTEXT} on Object Path: {OBJECT_PATH} value is {POWER_CAP}",
                "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
                "OBJECT_PATH", objectPath, "POWER_CAP", powerCap);
            if (type == DeviceType::CPU)
            {
                auto& moduleInfo = platformCpuGpuMap[locationContext];
                moduleInfo.cpuInfo.powerCapValue = powerCap;
                syncPowerCapForAllGpus(locationContext);
            }
            else if (type == DeviceType::GPU)
            {
                if (powerCap == PowerCapInvalid)
                {
                    lg2::info(
                        "Device : {DEVICE} on LocationContext: {LOCATION_CONTEXT} went offline, PowerCapValue : {POWER_CAP}",
                        "DEVICE", deviceName, "LOCATION_CONTEXT",
                        locationContext, "POWER_CAP", powerCap);
                    return;
                }

                auto& gpuInfo = platformCpuGpuMap[locationContext]
                                    .connectedGpuInfos[deviceName];
                gpuInfo.powerCapValue = powerCap;
                setPowerCapOnGpu(deviceName, locationContext);
            }
        }
    }
}

void GpuCpuPowerSync::powerCapInterfaceAddedHandler(
    DeviceType type, const std::string& deviceName,
    const std::string& locationContext,
    const std::string& processorPowerLimitPath,
    sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    utils::InterfaceMap interfaces;
    msg.read(objPath, interfaces);
    std::string objectPath = objPath.str;
    lg2::info(
        "Recieved signal for Power cap interface added for Device : {DEVICE_NAME} on {LOCATION_CONTEXT} on {OBJECT_PATH}",
        "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
        "OBJECT_PATH", objectPath);

    auto ifaceIt = interfaces.find(PowerCapInterface);
    if (ifaceIt == interfaces.end())
    {
        return;
    }

    auto propIt = ifaceIt->second.find(PowerCapProperty);
    if (propIt == ifaceIt->second.end())
    {
        return;
    }

    uint32_t powerCap = std::get<uint32_t>(propIt->second);
    lg2::info(
        "Power cap added on Device: {DEVICE_NAME} on {LOCATION_CONTEXT} on {OBJECT_PATH} value is {POWER_CAP}",
        "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
        "OBJECT_PATH", objectPath, "POWER_CAP", powerCap);

    bus_->async_method_call(
        [this, type, processorPowerLimitPath, deviceName, locationContext,
         powerCap](boost::system::error_code ec,
                   std::map<std::string, std::vector<std::string>>
                       servicesAndInterfaces) {
        if (ec)
        {
            lg2::error(
                "powerCapInterfaceAddedHandler:: GetObject call for {PROCESSOR_POWER_LIMIT_PATH} returned {ERROR}",
                "PROCESSOR_POWER_LIMIT_PATH", processorPowerLimitPath, "ERROR",
                ec.message());
            return;
        }

        if (servicesAndInterfaces.empty())
        {
            lg2::error(
                "powerCapInterfaceAddedHandler:: No service found for {PROCESSOR_POWER_LIMIT_PATH}",
                "PROCESSOR_POWER_LIMIT_PATH", processorPowerLimitPath);
            return;
        }

        std::string serviceName = servicesAndInterfaces.begin()->first;
        lg2::info(
            "Power cap interface added for: {DEVICE_NAME} on {LOCATION_CONTEXT} path: {PROCESSOR_POWER_LIMIT_PATH} service: {SERVICE_NAME} value: {POWER_CAP}",
            "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
            "PROCESSOR_POWER_LIMIT_PATH", processorPowerLimitPath,
            "SERVICE_NAME", serviceName, "POWER_CAP", powerCap);

        if (type == DeviceType::GPU)
        {
            auto& gpuInfo = platformCpuGpuMap[locationContext]
                                .connectedGpuInfos[deviceName];
            updateDeviceInfo(gpuInfo, processorPowerLimitPath, serviceName,
                             PowerCapInterface, PowerCapProperty, powerCap);
            setPowerCapOnGpu(deviceName, locationContext);
        }
        else if (type == DeviceType::CPU)
        {
            auto& cpuInfo = platformCpuGpuMap[locationContext].cpuInfo;
            updateDeviceInfo(cpuInfo, processorPowerLimitPath, serviceName,
                             PowerCapInterface, PowerCapProperty, powerCap);
            syncPowerCapForAllGpus(locationContext);
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject",
        processorPowerLimitPath, std::vector<std::string>{PowerCapInterface});
}

} // namespace nvidia::power::balancer
