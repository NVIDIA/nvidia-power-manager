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

#include "gpuCpuPowerSync.hpp"

namespace nvidia::power::balancer
{
GpuCpuPowerSync::GpuCpuPowerSync(
    std::shared_ptr<sdbusplus::asio::connection> bus) :
    bus_(bus),
    emDeviceAddedSignal(
        static_cast<sdbusplus::bus::bus&>(*bus),
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/inventory") +
            sdbusplus::bus::match::rules::sender(
                "xyz.openbmc_project.EntityManager"),
        std::bind_front(&GpuCpuPowerSync::emDeviceAddedHandler, this))
{
    utils::InterfaceList ifaces = {CpuInterface, GpuInterface};

    utils::getSubTreeAsync(bus_, "/xyz/openbmc_project/inventory", 0, ifaces,
                           [this](boost::system::error_code ec,
                                  utils::GetSubTreeResponse subtree) {
        if (ec)
        {
            lg2::error("Failed to get initial device subtree: {ERROR}", "ERROR",
                       ec.message());
            return;
        }

        for (const auto& [objectPath, serviceMap] : subtree)
        {
            for (const auto& [serviceName, interfaces] : serviceMap)

            { // Check if this object has locationContext and (CPU or GPU)
                bool hasLocationContext =
                    std::find(interfaces.begin(), interfaces.end(),
                              LocationContextInterface) != interfaces.end();
                bool hasCPU = std::find(interfaces.begin(), interfaces.end(),
                                        CpuInterface) != interfaces.end();
                bool hasGPU = std::find(interfaces.begin(), interfaces.end(),
                                        GpuInterface) != interfaces.end();
                lg2::info(
                    "GpuCpuPowerSync::getSubTreeAsync objectPath : {OBJECT_PATH} serviceName : {SERVICE_NAME} hasLocationContext : {HAS_LOCATION_CONTEXT} hasCPU : {HAS_CPU} hasGPU : {HAS_GPU}",
                    "OBJECT_PATH", objectPath, "SERVICE_NAME", serviceName,
                    "HAS_LOCATION_CONTEXT", hasLocationContext, "HAS_CPU",
                    hasCPU, "HAS_GPU", hasGPU);

                if (hasLocationContext && (hasCPU || hasGPU))
                {
                    if (hasCPU && hasGPU)
                    {
                        lg2::error(
                            "GpuCpuPowerSync::getSubTreeAsync CPU and GPU interfaces both found on Object Path: {OBJECT_PATH}",
                            "OBJECT_PATH", objectPath);
                        continue;
                    }

                    DeviceType type = hasGPU ? DeviceType::GPU
                                             : DeviceType::CPU;
                    utils::getPropertyAsync<std::string>(
                        bus_, serviceName, objectPath, LocationContextInterface,
                        "LocationContext",
                        std::bind_front(
                            &GpuCpuPowerSync::onLocationContextFetched, this,
                            type, objectPath));
                }
            }
        }
    });
}

void GpuCpuPowerSync::emDeviceAddedHandler(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    utils::InterfaceMap interfaces;
    try
    {
        msg.read(objPath, interfaces);
    }
    catch (const std::exception& e)
    {
        lg2::error("emDeviceAddedHandler:: Failed to read: {ERROR}", "ERROR",
                   e.what());
        return;
    }
    std::string objectPath = objPath.str;

    // Scenario 1: CPU or GPU interface directly in signal
    if (interfaces.find(CpuInterface) != interfaces.end())
    {
        lg2::info("emDeviceAddedHandler:: CPU interface found on {OBJECT_PATH}",
                  "OBJECT_PATH", objectPath);
        fetchLocationContextAndDiscover(DeviceType::CPU, objectPath);
        return;
    }

    if (interfaces.find(GpuInterface) != interfaces.end())
    {
        lg2::info("emDeviceAddedHandler:: GPU interface found on {OBJECT_PATH}",
                  "OBJECT_PATH", objectPath);
        fetchLocationContextAndDiscover(DeviceType::GPU, objectPath);
        return;
    }

    // Scenario 2: LocationContext interface - need to determine device type
    auto locCtxIt = interfaces.find(LocationContextInterface);
    if (locCtxIt == interfaces.end())
    {
        return;
    }

    auto locPropIt = locCtxIt->second.find("LocationContext");
    if (locPropIt == locCtxIt->second.end())
    {
        return;
    }

    std::string locationContext;
    try
    {
        locationContext = std::get<std::string>(locPropIt->second);
    }
    catch (const std::bad_variant_access&)
    {
        lg2::error("emDeviceAddedHandler:: Failed to extract LocationContext");
        return;
    }

    if (locationContext.empty())
    {
        lg2::error(
            "emDeviceAddedHandler:: Empty LocationContext on {OBJECT_PATH}",
            "OBJECT_PATH", objectPath);
        return;
    }

    lg2::info("emDeviceAddedHandler:: LocationContext {LOC} on {OBJECT_PATH}",
              "LOC", locationContext, "OBJECT_PATH", objectPath);

    resolveDeviceTypeAndDiscover(objectPath, locationContext);
}

void GpuCpuPowerSync::fetchLocationContextAndDiscover(
    DeviceType type, const std::string& objectPath)
{
    utils::getPropertyAsync<std::string>(
        bus_, EntityManagerService, objectPath, LocationContextInterface,
        "LocationContext",
        std::bind_front(&GpuCpuPowerSync::onLocationContextFetched, this, type,
                        objectPath));
}

void GpuCpuPowerSync::onLocationContextFetched(DeviceType type,
                                               const std::string& objectPath,
                                               boost::system::error_code ec,
                                               std::string locationContext)
{
    if (ec)
    {
        lg2::error(
            "onLocationContextFetched:: Failed for {OBJECT_PATH}: {ERROR}",
            "OBJECT_PATH", objectPath, "ERROR", ec.message());
        return;
    }

    const char* typeStr = (type == DeviceType::GPU) ? "GPU" : "CPU";
    lg2::info(
        "onLocationContextFetched:: {TYPE} on {OBJECT_PATH} LocationContext: {LOC}",
        "TYPE", typeStr, "OBJECT_PATH", objectPath, "LOC", locationContext);

    if (type == DeviceType::CPU)
    {
        discoverCpuViaPowerLimitAssociation(objectPath, locationContext);
    }
    else
    {
        discoverGpuViaCpuPowerAssociation(objectPath, locationContext);
    }
}

void GpuCpuPowerSync::resolveDeviceTypeAndDiscover(
    const std::string& objectPath, const std::string& locationContext)
{
    bus_->async_method_call(
        [this, objectPath,
         locationContext](boost::system::error_code ec,
                          const std::map<std::string, std::vector<std::string>>&
                              servicesAndInterfaces) {
        onDeviceTypeResolved(objectPath, locationContext, ec,
                             servicesAndInterfaces);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", objectPath,
        std::vector<std::string>{CpuInterface, GpuInterface});
}

void GpuCpuPowerSync::onDeviceTypeResolved(
    const std::string& objectPath, const std::string& locationContext,
    boost::system::error_code ec,
    const std::map<std::string, std::vector<std::string>>&
        servicesAndInterfaces)
{
    if (ec)
    {
        lg2::debug(
            "onDeviceTypeResolved:: No CPU/GPU interface on {OBJECT_PATH}",
            "OBJECT_PATH", objectPath);
        return;
    }

    auto hasCpu = std::any_of(servicesAndInterfaces.begin(),
                              servicesAndInterfaces.end(),
                              [](const auto& pair) {
        return std::find(pair.second.begin(), pair.second.end(),
                         CpuInterface) != pair.second.end();
    });

    auto hasGpu = std::any_of(servicesAndInterfaces.begin(),
                              servicesAndInterfaces.end(),
                              [](const auto& pair) {
        return std::find(pair.second.begin(), pair.second.end(),
                         GpuInterface) != pair.second.end();
    });

    if (hasCpu)
    {
        lg2::info("onDeviceTypeResolved:: CPU on {OBJECT_PATH}", "OBJECT_PATH",
                  objectPath);
        discoverCpuViaPowerLimitAssociation(objectPath, locationContext);
    }
    else if (hasGpu)
    {
        lg2::info("onDeviceTypeResolved:: GPU on {OBJECT_PATH}", "OBJECT_PATH",
                  objectPath);
        discoverGpuViaCpuPowerAssociation(objectPath, locationContext);
    }
}

void GpuCpuPowerSync::processAssociationEndpoints(
    DeviceType type, const std::string& deviceName,
    const std::string& locationContext,
    const std::vector<std::string>& endpoints)
{
    if (endpoints.empty())
    {
        return;
    }

    if (type == DeviceType::GPU)
    {
        const auto& endpoint = endpoints.front();
        if (platformCpuGpuMap[locationContext]
                .connectedGpuInfos[deviceName]
                .objectPath != endpoint)
        {
            lg2::info(
                "processAssociationEndpoints:: Discovering GPU {DEVICE_NAME} endpoint: {ENDPOINT}",
                "DEVICE_NAME", deviceName, "ENDPOINT", endpoint);
            discoverGpuDevice(deviceName, locationContext, endpoint);
        }
        return;
    }

    if (type == DeviceType::CPU)
    {
        auto it = std::find_if(endpoints.begin(), endpoints.end(),
                               [](const std::string& ep) {
            return ep.find("TDPPowerVolatile") != std::string::npos;
        });

        if (it == endpoints.end())
        {
            lg2::debug(
                "processAssociationEndpoints:: No TDPPowerVolatile for CPU {DEVICE_NAME}",
                "DEVICE_NAME", deviceName);
            return;
        }

        if (platformCpuGpuMap[locationContext].cpuInfo.objectPath != *it)
        {
            lg2::info(
                "processAssociationEndpoints:: Discovering CPU {DEVICE_NAME} endpoint: {ENDPOINT}",
                "DEVICE_NAME", deviceName, "ENDPOINT", *it);
            discoverCpuDevice(deviceName, locationContext, *it);
        }
    }
}

void GpuCpuPowerSync::associationsInterfaceAddedHandler(
    DeviceType type, const std::string& deviceName,
    const std::string& locationContext,
    [[maybe_unused]] const std::string& associationPath,
    sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    utils::InterfaceMap interfaces;
    msg.read(objPath, interfaces);

    lg2::info(
        "associationsInterfaceAddedHandler:: Signal for {DEVICE_NAME} on {LOCATION_CONTEXT} path: {OBJECT_PATH}",
        "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
        "OBJECT_PATH", objPath.str);

    auto ifaceIt = interfaces.find(AssociationInterface);
    if (ifaceIt == interfaces.end())
    {
        return;
    }

    auto propIt = ifaceIt->second.find("endpoints");
    if (propIt == ifaceIt->second.end())
    {
        return;
    }

    auto endpoints = std::get<std::vector<std::string>>(propIt->second);
    processAssociationEndpoints(type, deviceName, locationContext, endpoints);
}

void GpuCpuPowerSync::associationsPropertyChangedHandler(
    DeviceType type, const std::string& deviceName,
    const std::string& locationContext, sdbusplus::message::message& msg)
{
    std::string interface;
    utils::PropertyMap properties;
    msg.read(interface, properties);

    lg2::info(
        "associationsPropertyChangedHandler:: Signal for {DEVICE_NAME} on {LOCATION_CONTEXT}",
        "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext);

    if (interface != AssociationInterface)
    {
        return;
    }

    auto propIt = properties.find("endpoints");
    if (propIt == properties.end())
    {
        return;
    }

    auto endpoints = std::get<std::vector<std::string>>(propIt->second);
    processAssociationEndpoints(type, deviceName, locationContext, endpoints);
}

void GpuCpuPowerSync::discoverGpuViaCpuPowerAssociation(
    const std::string& objectPath, const std::string& locationContext)
{
    std::string deviceName =
        std::filesystem::path(objectPath).filename().string();
    std::string associationPath = objectPath + "/GPU_copy_Cpu_Power";
    lg2::info(
        "discoverGpuViaCpuPowerAssociation:: GPU copy CPU power association for Device: {DEVICE_NAME} locationContext: {LOCATION_CONTEXT} is {ASSOCIATION_PATH}",
        "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
        "ASSOCIATION_PATH", associationPath);
    if (associationsInterfaceAddedSignals.find(deviceName) ==
        associationsInterfaceAddedSignals.end())
    {
        associationsInterfaceAddedSignals.emplace(
            deviceName,
            sdbusplus::bus::match_t(
                static_cast<sdbusplus::bus::bus&>(*bus_),
                sdbusplus::bus::match::rules::interfacesAdded("/") +
                    sdbusplus::bus::match::rules::argNpath(0, associationPath) +
                    sdbusplus::bus::match::rules::sender(
                        "xyz.openbmc_project.ObjectMapper"),
                std::bind_front(
                    &GpuCpuPowerSync::associationsInterfaceAddedHandler, this,
                    DeviceType::GPU, deviceName, locationContext,
                    associationPath)));
    }
    if (associationsPropertyChangedSignals.find(deviceName) ==
        associationsPropertyChangedSignals.end())
    {
        associationsPropertyChangedSignals.emplace(
            deviceName,
            sdbusplus::bus::match_t(
                static_cast<sdbusplus::bus::bus&>(*bus_),
                sdbusplus::bus::match::rules::propertiesChanged(
                    associationPath, AssociationInterface),
                std::bind_front(
                    &GpuCpuPowerSync::associationsPropertyChangedHandler, this,
                    DeviceType::GPU, deviceName, locationContext)));
    }

    utils::getPropertyAsync<std::vector<std::string>>(
        bus_, ObjectMapperService, associationPath, AssociationInterface,
        EndpointProperty,
        [this, locationContext, deviceName](
            boost::system::error_code ec, std::vector<std::string> endpoints) {
        if (ec)
        {
            lg2::error(
                "discoverGpuViaCpuPowerAssociation:: Failed to get object mapper: {ERROR}",
                "ERROR", ec.message());
            return;
        }
        else
        {
            for (const auto& endpoint : endpoints)
            {
                lg2::info(
                    "discoverGpuViaCpuPowerAssociation:: Found Associated GPU Copy CPU Power associated object path: {ENDPOINT} for Device: {DEVICE_NAME} and locationContext: {LOCATION_CONTEXT}",
                    "ENDPOINT", endpoint, "DEVICE_NAME", deviceName,
                    "LOCATION_CONTEXT", locationContext);
                discoverGpuDevice(deviceName, locationContext, endpoint);
                break;
            }
        }
    });
}

void GpuCpuPowerSync::updateDeviceInfo(DeviceInfo& deviceInfo,
                                       const std::string& objectPath,
                                       const std::string& serviceName,
                                       const std::string& interfaceName,
                                       const std::string& propertyName,
                                       uint32_t powerCapValue)
{
    deviceInfo.powerCapValue = powerCapValue;
    deviceInfo.serviceName = serviceName;
    deviceInfo.objectPath = objectPath;
    deviceInfo.interfaceName = interfaceName;
    deviceInfo.propertyName = propertyName;
}

void GpuCpuPowerSync::registerPowerCapSignalHandlers(
    DeviceType type, DeviceInfo& deviceInfo, const std::string& deviceName,
    const std::string& locationContext, const std::string& powerLimitPath)
{
    deviceInfo.powerCapChangedSignal =
        std::make_unique<sdbusplus::bus::match_t>(
            static_cast<sdbusplus::bus::bus&>(*bus_),
            sdbusplus::bus::match::rules::propertiesChanged(powerLimitPath,
                                                            PowerCapInterface),
            std::bind_front(&GpuCpuPowerSync::powerCapChangedHandler, this,
                            type, deviceName, locationContext));

    deviceInfo.powerCapInterfaceAddedSignal =
        std::make_unique<sdbusplus::bus::match_t>(
            static_cast<sdbusplus::bus::bus&>(*bus_),
            sdbusplus::bus::match::rules::interfacesAdded("/") +
                sdbusplus::bus::match::rules::argNpath(0, powerLimitPath),
            std::bind_front(&GpuCpuPowerSync::powerCapInterfaceAddedHandler,
                            this, type, deviceName, locationContext,
                            powerLimitPath));
}

std::string GpuCpuPowerSync::extractServiceName(
    [[maybe_unused]] DeviceType type,
    const std::map<std::string, std::vector<std::string>>&
        servicesAndInterfaces)
{
    if (servicesAndInterfaces.empty())
    {
        return "";
    }
    return servicesAndInterfaces.begin()->first;
}

void GpuCpuPowerSync::onServiceDiscovered(
    DeviceType type, const std::string& deviceName,
    const std::string& locationContext, const std::string& powerLimitPath,
    boost::system::error_code ec,
    const std::map<std::string, std::vector<std::string>>&
        servicesAndInterfaces)
{
    const char* typeStr = (type == DeviceType::GPU) ? "GPU" : "CPU";

    if (ec)
    {
        lg2::error(
            "discover{TYPE}Device:: PowerCap interface not found on {PATH}: {ERROR}",
            "TYPE", typeStr, "PATH", powerLimitPath, "ERROR", ec.message());
        return;
    }

    std::string serviceName = extractServiceName(type, servicesAndInterfaces);
    if (serviceName.empty())
    {
        lg2::error(
            "discover{TYPE}Device:: No service provides PowerCap for {PATH}",
            "TYPE", typeStr, "PATH", powerLimitPath);
        return;
    }

    lg2::info(
        "discover{TYPE}Device:: Found PowerCap interface on {PATH} for Device: {DEVICE_NAME} Location Context: {LOCATION_CONTEXT}",
        "TYPE", typeStr, "PATH", powerLimitPath, "DEVICE_NAME", deviceName,
        "LOCATION_CONTEXT", locationContext);

    utils::getPropertyAsync<uint32_t>(
        bus_, serviceName, powerLimitPath, PowerCapInterface, PowerCapProperty,
        std::bind_front(&GpuCpuPowerSync::onPowerCapRetrieved, this, type,
                        deviceName, locationContext, powerLimitPath,
                        serviceName));
}

void GpuCpuPowerSync::onPowerCapRetrieved(DeviceType type,
                                          const std::string& deviceName,
                                          const std::string& locationContext,
                                          const std::string& powerLimitPath,
                                          const std::string& serviceName,
                                          boost::system::error_code ec,
                                          uint32_t powerCap)
{
    const char* typeStr = (type == DeviceType::GPU) ? "GPU" : "CPU";

    if (ec)
    {
        lg2::error("discover{TYPE}Device:: Failed to get power cap: {ERROR}",
                   "TYPE", typeStr, "ERROR", ec.message());
        powerCap = DefaultPowerCap;
    }
    else
    {
        lg2::info("discover{TYPE}Device:: Power cap: {POWER_CAP}", "TYPE",
                  typeStr, "POWER_CAP", powerCap);
    }

    DeviceInfo& deviceInfo =
        (type == DeviceType::GPU)
            ? platformCpuGpuMap[locationContext].connectedGpuInfos[deviceName]
            : platformCpuGpuMap[locationContext].cpuInfo;

    updateDeviceInfo(deviceInfo, powerLimitPath, serviceName, PowerCapInterface,
                     PowerCapProperty, powerCap);

    if (type == DeviceType::GPU)
    {
        setPowerCapOnGpu(deviceName, locationContext);
    }
    else
    {
        syncPowerCapForAllGpus(locationContext);
    }
}

void GpuCpuPowerSync::discoverGpuDevice(const std::string& deviceName,
                                        const std::string& locationContext,
                                        const std::string& powerLimitPath)
{
    auto& gpuInfo =
        platformCpuGpuMap[locationContext].connectedGpuInfos[deviceName];
    registerPowerCapSignalHandlers(DeviceType::GPU, gpuInfo, deviceName,
                                   locationContext, powerLimitPath);

    bus_->async_method_call(
        [this, deviceName, locationContext,
         powerLimitPath](boost::system::error_code ec,
                         const std::map<std::string, std::vector<std::string>>&
                             servicesAndInterfaces) {
        onServiceDiscovered(DeviceType::GPU, deviceName, locationContext,
                            powerLimitPath, ec, servicesAndInterfaces);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", powerLimitPath,
        std::vector<std::string>{PowerCapInterface});
}

void GpuCpuPowerSync::discoverCpuViaPowerLimitAssociation(
    const std::string& objectPath, const std::string& locationContext)
{
    std::string deviceName =
        std::filesystem::path(objectPath).filename().string();
    std::string associationPath = objectPath + "/power_controls";

    lg2::info(
        "discoverCpuViaPowerLimitAssociation:: Power Controls association for Device: {DEVICE_NAME} locationContext: {LOCATION_CONTEXT} is {ASSOCIATION_PATH}",
        "DEVICE_NAME", deviceName, "LOCATION_CONTEXT", locationContext,
        "ASSOCIATION_PATH", associationPath);

    if (associationsInterfaceAddedSignals.find(deviceName) ==
        associationsInterfaceAddedSignals.end())
    {
        associationsInterfaceAddedSignals.emplace(
            deviceName,
            sdbusplus::bus::match_t(
                static_cast<sdbusplus::bus::bus&>(*bus_),
                sdbusplus::bus::match::rules::interfacesAdded("/") +
                    sdbusplus::bus::match::rules::argNpath(0, associationPath) +
                    sdbusplus::bus::match::rules::sender(
                        "xyz.openbmc_project.ObjectMapper"),
                std::bind_front(
                    &GpuCpuPowerSync::associationsInterfaceAddedHandler, this,
                    DeviceType::CPU, deviceName, locationContext,
                    associationPath)));
    }

    if (associationsPropertyChangedSignals.find(deviceName) ==
        associationsPropertyChangedSignals.end())
    {
        associationsPropertyChangedSignals.emplace(
            deviceName,
            sdbusplus::bus::match_t(
                static_cast<sdbusplus::bus::bus&>(*bus_),
                sdbusplus::bus::match::rules::propertiesChanged(
                    associationPath, AssociationInterface),
                std::bind_front(
                    &GpuCpuPowerSync::associationsPropertyChangedHandler, this,
                    DeviceType::CPU, deviceName, locationContext)));
    }

    utils::getPropertyAsync<std::vector<std::string>>(
        bus_, ObjectMapperService, associationPath, AssociationInterface,
        EndpointProperty,
        [this, locationContext, deviceName](
            boost::system::error_code ec, std::vector<std::string> endpoints) {
        if (ec)
        {
            lg2::error(
                "discoverCpuViaPowerLimitAssociation:: Failed to get object mapper: {ERROR}",
                "ERROR", ec.message());
            return;
        }
        else
        {
            for (const auto& endpoint : endpoints)
            {
                if (endpoint.find("TDPPowerVolatile") != std::string::npos)
                {
                    lg2::info(
                        "discoverCpuViaPowerLimitAssociation:: TDPPowerVolatile found on Object Path: {OBJECT_PATH} for Device: {DEVICE_NAME} locationContext: {LOCATION_CONTEXT}",
                        "OBJECT_PATH", endpoint, "DEVICE_NAME", deviceName,
                        "LOCATION_CONTEXT", locationContext);
                    discoverCpuDevice(deviceName, locationContext, endpoint);
                    break;
                }
            }
        }
    });
}

void GpuCpuPowerSync::discoverCpuDevice(const std::string& deviceName,
                                        const std::string& locationContext,
                                        const std::string& powerLimitPath)
{
    auto& cpuInfo = platformCpuGpuMap[locationContext].cpuInfo;
    registerPowerCapSignalHandlers(DeviceType::CPU, cpuInfo, deviceName,
                                   locationContext, powerLimitPath);

    bus_->async_method_call(
        [this, deviceName, locationContext,
         powerLimitPath](boost::system::error_code ec,
                         const std::map<std::string, std::vector<std::string>>&
                             servicesAndInterfaces) {
        onServiceDiscovered(DeviceType::CPU, deviceName, locationContext,
                            powerLimitPath, ec, servicesAndInterfaces);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", powerLimitPath,
        std::vector<std::string>{PowerCapInterface});
}

} // namespace nvidia::power::balancer
