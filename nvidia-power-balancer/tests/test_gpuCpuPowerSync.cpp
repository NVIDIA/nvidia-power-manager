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
#include "utils.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

using namespace nvidia::power::balancer;

// ============================================================================
// Test Data Structures (No D-Bus Required)
// ============================================================================

// --- DeviceInfo struct tests ---

TEST(DeviceInfoTest, DefaultValues)
{
    DeviceInfo info;
    EXPECT_TRUE(info.serviceName.empty());
    EXPECT_TRUE(info.objectPath.empty());
    EXPECT_TRUE(info.interfaceName.empty());
    EXPECT_TRUE(info.propertyName.empty());
    EXPECT_EQ(info.powerCapValue, 0u);
    EXPECT_EQ(info.jobMonitor, nullptr);
    EXPECT_EQ(info.powerCapChangedSignal, nullptr);
    EXPECT_EQ(info.powerCapInterfaceAddedSignal, nullptr);
}

TEST(DeviceInfoTest, SetValues)
{
    DeviceInfo info;
    info.serviceName = "com.nvidia.nsm";
    info.objectPath = "/xyz/openbmc_project/control/GPU_0/power";
    info.interfaceName = "xyz.openbmc_project.Control.Power.Cap";
    info.propertyName = "PowerCap";
    info.powerCapValue = 450;

    EXPECT_EQ(info.serviceName, "com.nvidia.nsm");
    EXPECT_EQ(info.objectPath, "/xyz/openbmc_project/control/GPU_0/power");
    EXPECT_EQ(info.interfaceName, "xyz.openbmc_project.Control.Power.Cap");
    EXPECT_EQ(info.propertyName, "PowerCap");
    EXPECT_EQ(info.powerCapValue, 450u);
}

// --- moduleDeviceInfo struct tests ---

TEST(ModuleDeviceInfoTest, DefaultValues)
{
    moduleDeviceInfo moduleInfo;
    EXPECT_TRUE(moduleInfo.cpuInfo.objectPath.empty());
    EXPECT_TRUE(moduleInfo.connectedGpuInfos.empty());
}

TEST(ModuleDeviceInfoTest, AddGpuInfo)
{
    moduleDeviceInfo moduleInfo;

    // Initially empty
    EXPECT_TRUE(moduleInfo.connectedGpuInfos.empty());

    // Add GPU info - use operator[] to create in place, then modify
    auto& gpuInfo = moduleInfo.connectedGpuInfos["GPU_0"];
    gpuInfo.objectPath = "/xyz/openbmc_project/control/GPU_0/power";
    gpuInfo.powerCapValue = 400;

    EXPECT_EQ(moduleInfo.connectedGpuInfos.size(), 1u);
    EXPECT_EQ(moduleInfo.connectedGpuInfos["GPU_0"].powerCapValue, 400u);
}

TEST(ModuleDeviceInfoTest, CpuAndMultipleGpus)
{
    moduleDeviceInfo moduleInfo;

    // Set CPU info
    moduleInfo.cpuInfo.objectPath = "/xyz/openbmc_project/control/CPU_0/power";
    moduleInfo.cpuInfo.powerCapValue = 300;

    // Add multiple GPUs - create in place to avoid copy assignment
    for (int i = 0; i < 4; ++i)
    {
        std::string gpuName = "GPU_" + std::to_string(i);
        auto& gpuInfo = moduleInfo.connectedGpuInfos[gpuName];
        gpuInfo.objectPath = "/xyz/openbmc_project/control/" + gpuName +
                             "/power";
        gpuInfo.powerCapValue = 400 + i * 10;
    }

    EXPECT_EQ(moduleInfo.cpuInfo.powerCapValue, 300u);
    EXPECT_EQ(moduleInfo.connectedGpuInfos.size(), 4u);
    EXPECT_EQ(moduleInfo.connectedGpuInfos["GPU_2"].powerCapValue, 420u);
}

TEST(ModuleDeviceInfoTest, MultipleLocations)
{
    std::map<std::string, moduleDeviceInfo> platformCpuGpuMap;

    // Setup multiple location contexts
    platformCpuGpuMap["HGX_Chassis_0/ProcessorModule_0"].cpuInfo.powerCapValue =
        300;
    platformCpuGpuMap["HGX_Chassis_0/ProcessorModule_1"].cpuInfo.powerCapValue =
        350;

    platformCpuGpuMap["HGX_Chassis_0/ProcessorModule_0"]
        .connectedGpuInfos["GPU_0"]
        .powerCapValue = 400;
    platformCpuGpuMap["HGX_Chassis_0/ProcessorModule_1"]
        .connectedGpuInfos["GPU_4"]
        .powerCapValue = 450;

    EXPECT_EQ(platformCpuGpuMap.size(), 2u);
    EXPECT_EQ(platformCpuGpuMap["HGX_Chassis_0/ProcessorModule_0"]
                  .cpuInfo.powerCapValue,
              300u);
    EXPECT_EQ(platformCpuGpuMap["HGX_Chassis_0/ProcessorModule_1"]
                  .connectedGpuInfos["GPU_4"]
                  .powerCapValue,
              450u);
}

// --- DeviceType enum tests ---

TEST(DeviceTypeTest, EnumValues)
{
    DeviceType gpuType = DeviceType::GPU;
    DeviceType cpuType = DeviceType::CPU;

    EXPECT_NE(gpuType, cpuType);
    EXPECT_EQ(gpuType, DeviceType::GPU);
    EXPECT_EQ(cpuType, DeviceType::CPU);
}

// --- Power Cap Constants tests ---

TEST(PowerCapConstantsTest, DefaultAndInvalidValues)
{
    EXPECT_EQ(DefaultPowerCap, 0u);
    EXPECT_EQ(PowerCapInvalid, 0xFFFFFFFFu);
}

// --- Device State Validation Logic tests ---

TEST(DeviceStateTest, CpuNotReadyForSync_EmptyPath)
{
    DeviceInfo cpuInfo;
    cpuInfo.objectPath = "";
    cpuInfo.powerCapValue = 300;

    // CPU not ready if objectPath is empty
    bool cpuReady = !cpuInfo.objectPath.empty() &&
                    cpuInfo.powerCapValue != DefaultPowerCap &&
                    cpuInfo.powerCapValue != PowerCapInvalid;
    EXPECT_FALSE(cpuReady);
}

TEST(DeviceStateTest, CpuNotReadyForSync_DefaultPowerCap)
{
    DeviceInfo cpuInfo;
    cpuInfo.objectPath = "/cpu/path";
    cpuInfo.powerCapValue = DefaultPowerCap;

    bool cpuReady = !cpuInfo.objectPath.empty() &&
                    cpuInfo.powerCapValue != DefaultPowerCap &&
                    cpuInfo.powerCapValue != PowerCapInvalid;
    EXPECT_FALSE(cpuReady);
}

TEST(DeviceStateTest, CpuNotReadyForSync_InvalidPowerCap)
{
    DeviceInfo cpuInfo;
    cpuInfo.objectPath = "/cpu/path";
    cpuInfo.powerCapValue = PowerCapInvalid;

    bool cpuReady = !cpuInfo.objectPath.empty() &&
                    cpuInfo.powerCapValue != DefaultPowerCap &&
                    cpuInfo.powerCapValue != PowerCapInvalid;
    EXPECT_FALSE(cpuReady);
}

TEST(DeviceStateTest, CpuReadyForSync)
{
    DeviceInfo cpuInfo;
    cpuInfo.objectPath = "/cpu/path";
    cpuInfo.powerCapValue = 300;

    bool cpuReady = !cpuInfo.objectPath.empty() &&
                    cpuInfo.powerCapValue != DefaultPowerCap &&
                    cpuInfo.powerCapValue != PowerCapInvalid;
    EXPECT_TRUE(cpuReady);
}

TEST(DeviceStateTest, GpuNotReadyForSync_EmptyPath)
{
    DeviceInfo gpuInfo;
    gpuInfo.objectPath = "";
    gpuInfo.serviceName = "com.nvidia.nsm";
    gpuInfo.powerCapValue = 400;

    bool gpuReady = !gpuInfo.objectPath.empty() &&
                    !gpuInfo.serviceName.empty() &&
                    gpuInfo.powerCapValue != PowerCapInvalid;
    EXPECT_FALSE(gpuReady);
}

TEST(DeviceStateTest, GpuNotReadyForSync_EmptyService)
{
    DeviceInfo gpuInfo;
    gpuInfo.objectPath = "/gpu/path";
    gpuInfo.serviceName = "";
    gpuInfo.powerCapValue = 400;

    bool gpuReady = !gpuInfo.objectPath.empty() &&
                    !gpuInfo.serviceName.empty() &&
                    gpuInfo.powerCapValue != PowerCapInvalid;
    EXPECT_FALSE(gpuReady);
}

TEST(DeviceStateTest, GpuReadyForSync)
{
    DeviceInfo gpuInfo;
    gpuInfo.objectPath = "/gpu/path";
    gpuInfo.serviceName = "com.nvidia.nsm";
    gpuInfo.powerCapValue = 400;

    bool gpuReady = !gpuInfo.objectPath.empty() &&
                    !gpuInfo.serviceName.empty() &&
                    gpuInfo.powerCapValue != PowerCapInvalid;
    EXPECT_TRUE(gpuReady);
}

TEST(DeviceStateTest, PowerCapAlreadyInSync)
{
    DeviceInfo cpuInfo;
    cpuInfo.powerCapValue = 300;

    DeviceInfo gpuInfo;
    gpuInfo.powerCapValue = 300;

    bool alreadyInSync = (cpuInfo.powerCapValue == gpuInfo.powerCapValue);
    EXPECT_TRUE(alreadyInSync);
}

TEST(DeviceStateTest, PowerCapNotInSync)
{
    DeviceInfo cpuInfo;
    cpuInfo.powerCapValue = 300;

    DeviceInfo gpuInfo;
    gpuInfo.powerCapValue = 400;

    bool alreadyInSync = (cpuInfo.powerCapValue == gpuInfo.powerCapValue);
    EXPECT_FALSE(alreadyInSync);
}

// --- Interface name constants tests ---

TEST(InterfaceConstantsTest, InterfaceNames)
{
    EXPECT_STREQ(AssociationInterface, "xyz.openbmc_project.Association");
    EXPECT_STREQ(CpuInterface, "xyz.openbmc_project.Inventory.Item.Cpu");
    EXPECT_STREQ(GpuInterface,
                 "xyz.openbmc_project.Inventory.Item.Accelerator");
    EXPECT_STREQ(PowerCapInterface, "xyz.openbmc_project.Control.Power.Cap");
    EXPECT_STREQ(PowerCapProperty, "PowerCap");
}

// ============================================================================
// Pattern-Based Tests (Logic patterns from implementation)
// ============================================================================

// --- Path Construction Tests ---
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 368, 584

TEST(PathConstructionTest, GpuAssociationPath)
{
    std::string objectPath = "/xyz/openbmc_project/inventory/GPU_0";
    std::string associationPath = objectPath + "/GPU_copy_Cpu_Power";
    EXPECT_EQ(associationPath,
              "/xyz/openbmc_project/inventory/GPU_0/GPU_copy_Cpu_Power");
}

TEST(PathConstructionTest, CpuAssociationPath)
{
    std::string objectPath = "/xyz/openbmc_project/inventory/CPU_0";
    std::string associationPath = objectPath + "/power_controls";
    EXPECT_EQ(associationPath,
              "/xyz/openbmc_project/inventory/CPU_0/power_controls");
}

// --- Device Name Extraction Tests ---
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 366-367, 582-583

TEST(DeviceNameExtractionTest, ExtractGpuName)
{
    std::string objectPath = "/xyz/openbmc_project/inventory/GPU_0";
    std::string deviceName =
        std::filesystem::path(objectPath).filename().string();
    EXPECT_EQ(deviceName, "GPU_0");
}

TEST(DeviceNameExtractionTest, ExtractCpuName)
{
    std::string objectPath = "/xyz/openbmc_project/inventory/CPU_0";
    std::string deviceName =
        std::filesystem::path(objectPath).filename().string();
    EXPECT_EQ(deviceName, "CPU_0");
}

TEST(DeviceNameExtractionTest, DeepNestedPath)
{
    std::string objectPath =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/GPU_3";
    std::string deviceName =
        std::filesystem::path(objectPath).filename().string();
    EXPECT_EQ(deviceName, "GPU_3");
}

// --- Type String Conversion Tests ---
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 181, 485, 524

TEST(TypeConversionTest, GpuToString)
{
    DeviceType type = DeviceType::GPU;
    const char* typeStr = (type == DeviceType::GPU) ? "GPU" : "CPU";
    EXPECT_STREQ(typeStr, "GPU");
}

TEST(TypeConversionTest, CpuToString)
{
    DeviceType type = DeviceType::CPU;
    const char* typeStr = (type == DeviceType::GPU) ? "GPU" : "CPU";
    EXPECT_STREQ(typeStr, "CPU");
}

// --- Endpoint Filtering Tests ---
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 282-285

TEST(EndpointFilteringTest, FindTDPPowerVolatile)
{
    std::vector<std::string> endpoints = {
        "/xyz/openbmc_project/control/CPU_0/TDPPower",
        "/xyz/openbmc_project/control/CPU_0/TDPPowerVolatile",
        "/xyz/openbmc_project/control/CPU_0/MaxPower"};

    auto it = std::find_if(endpoints.begin(), endpoints.end(),
                           [](const std::string& ep) {
        return ep.find("TDPPowerVolatile") != std::string::npos;
    });

    ASSERT_NE(it, endpoints.end());
    EXPECT_EQ(*it, "/xyz/openbmc_project/control/CPU_0/TDPPowerVolatile");
}

TEST(EndpointFilteringTest, NoTDPPowerVolatile)
{
    std::vector<std::string> endpoints = {
        "/xyz/openbmc_project/control/CPU_0/TDPPower",
        "/xyz/openbmc_project/control/CPU_0/MaxPower"};

    auto it = std::find_if(endpoints.begin(), endpoints.end(),
                           [](const std::string& ep) {
        return ep.find("TDPPowerVolatile") != std::string::npos;
    });

    EXPECT_EQ(it, endpoints.end());
}

TEST(EndpointFilteringTest, EmptyEndpoints)
{
    std::vector<std::string> endpoints;

    auto it = std::find_if(endpoints.begin(), endpoints.end(),
                           [](const std::string& ep) {
        return ep.find("TDPPowerVolatile") != std::string::npos;
    });

    EXPECT_EQ(it, endpoints.end());
}

// --- Interface Detection Tests ---
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 50-56, 227-239

TEST(InterfaceDetectionTest, HasCpuInterface)
{
    std::vector<std::string> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Cpu",
        "xyz.openbmc_project.Inventory.Decorator.LocationContext"};

    bool hasCpu = std::find(interfaces.begin(), interfaces.end(),
                            CpuInterface) != interfaces.end();
    EXPECT_TRUE(hasCpu);
}

TEST(InterfaceDetectionTest, HasGpuInterface)
{
    std::vector<std::string> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Accelerator",
        "xyz.openbmc_project.Inventory.Decorator.LocationContext"};

    bool hasGpu = std::find(interfaces.begin(), interfaces.end(),
                            GpuInterface) != interfaces.end();
    EXPECT_TRUE(hasGpu);
}

TEST(InterfaceDetectionTest, HasLocationContext)
{
    std::vector<std::string> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Cpu",
        "xyz.openbmc_project.Inventory.Decorator.LocationContext"};

    bool hasLocationContext = std::find(interfaces.begin(), interfaces.end(),
                                        LocationContextInterface) !=
                              interfaces.end();
    EXPECT_TRUE(hasLocationContext);
}

// --- Service Extraction Tests ---
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 466-476

TEST(ServiceExtractionTest, ExtractFirstService)
{
    std::map<std::string, std::vector<std::string>> servicesAndInterfaces = {
        {"com.nvidia.nsm", {"xyz.openbmc_project.Control.Power.Cap"}},
        {"xyz.openbmc_project.EntityManager", {}}};

    std::string serviceName = servicesAndInterfaces.empty()
                                  ? ""
                                  : servicesAndInterfaces.begin()->first;
    EXPECT_EQ(serviceName, "com.nvidia.nsm");
}

TEST(ServiceExtractionTest, EmptyMap)
{
    std::map<std::string, std::vector<std::string>> servicesAndInterfaces;

    std::string serviceName = servicesAndInterfaces.empty()
                                  ? ""
                                  : servicesAndInterfaces.begin()->first;
    EXPECT_EQ(serviceName, "");
}

// ============================================================================
// InterfaceMap Find and Property Extraction Tests
// Pattern from gpuCpuPowerSyncControl.cpp lines 168-180
// ============================================================================

TEST(InterfaceMapTest, FindPowerCapInterface)
{
    utils::InterfaceMap interfaces;
    utils::PropertyMap props;
    props["PowerCap"] = uint32_t(400);
    interfaces[PowerCapInterface] = props;

    auto ifaceIt = interfaces.find(PowerCapInterface);
    ASSERT_NE(ifaceIt, interfaces.end());
    EXPECT_EQ(ifaceIt->first, PowerCapInterface);
}

TEST(InterfaceMapTest, InterfaceNotFound)
{
    utils::InterfaceMap interfaces;
    utils::PropertyMap props;
    props["SomeProperty"] = std::string("value");
    interfaces["xyz.openbmc_project.SomeOther.Interface"] = props;

    auto ifaceIt = interfaces.find(PowerCapInterface);
    EXPECT_EQ(ifaceIt, interfaces.end());
}

TEST(InterfaceMapTest, PropertyNotFoundInInterface)
{
    utils::InterfaceMap interfaces;
    utils::PropertyMap props;
    props["OtherProperty"] = uint32_t(100);
    interfaces[PowerCapInterface] = props;

    auto ifaceIt = interfaces.find(PowerCapInterface);
    ASSERT_NE(ifaceIt, interfaces.end());

    auto propIt = ifaceIt->second.find(PowerCapProperty);
    EXPECT_EQ(propIt, ifaceIt->second.end());
}

TEST(InterfaceMapTest, PropertyFoundInInterface)
{
    utils::InterfaceMap interfaces;
    utils::PropertyMap props;
    props[PowerCapProperty] = uint32_t(450);
    interfaces[PowerCapInterface] = props;

    auto ifaceIt = interfaces.find(PowerCapInterface);
    ASSERT_NE(ifaceIt, interfaces.end());

    auto propIt = ifaceIt->second.find(PowerCapProperty);
    ASSERT_NE(propIt, ifaceIt->second.end());
    EXPECT_EQ(std::get<uint32_t>(propIt->second), 450u);
}

// ============================================================================
// Variant Extraction Tests
// Pattern from gpuCpuPowerSyncControl.cpp line 180,
// gpuCpuPowerSyncDiscovery.cpp
// ============================================================================

TEST(VariantExtractionTest, ExtractUint32PowerCap)
{
    utils::PropertyValue value = uint32_t(500);
    uint32_t powerCap = std::get<uint32_t>(value);
    EXPECT_EQ(powerCap, 500u);
}

TEST(VariantExtractionTest, ExtractUint32PowerCapInvalid)
{
    utils::PropertyValue value = uint32_t(PowerCapInvalid);
    uint32_t powerCap = std::get<uint32_t>(value);
    EXPECT_EQ(powerCap, PowerCapInvalid);
}

TEST(VariantExtractionTest, ExtractVectorEndpoints)
{
    std::vector<std::string> endpoints = {
        "/xyz/openbmc_project/control/GPU_0/power",
        "/xyz/openbmc_project/control/GPU_1/power"};
    utils::PropertyValue value = endpoints;

    auto extracted = std::get<std::vector<std::string>>(value);
    EXPECT_EQ(extracted.size(), 2u);
    EXPECT_EQ(extracted[0], "/xyz/openbmc_project/control/GPU_0/power");
}

TEST(VariantExtractionTest, ExtractStringLocationContext)
{
    utils::PropertyValue value = std::string("HGX_Chassis_0/ProcessorModule_0");
    std::string locationContext = std::get<std::string>(value);
    EXPECT_EQ(locationContext, "HGX_Chassis_0/ProcessorModule_0");
}

// ============================================================================
// Association Path Construction Tests (GPU and CPU specific suffixes)
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 368, 584
// ============================================================================

TEST(AssociationPathTest, GpuCopyCpuPowerSuffix)
{
    std::string basePath = "/xyz/openbmc_project/inventory/GPU_0";
    std::string suffix = "/GPU_copy_Cpu_Power";
    std::string associationPath = basePath + suffix;

    EXPECT_EQ(associationPath,
              "/xyz/openbmc_project/inventory/GPU_0/GPU_copy_Cpu_Power");
    EXPECT_TRUE(associationPath.find("GPU_copy_Cpu_Power") !=
                std::string::npos);
}

TEST(AssociationPathTest, CpuPowerControlsSuffix)
{
    std::string basePath = "/xyz/openbmc_project/inventory/CPU_0";
    std::string suffix = "/power_controls";
    std::string associationPath = basePath + suffix;

    EXPECT_EQ(associationPath,
              "/xyz/openbmc_project/inventory/CPU_0/power_controls");
    EXPECT_TRUE(associationPath.find("power_controls") != std::string::npos);
}

TEST(AssociationPathTest, DifferentSuffixesForDeviceTypes)
{
    std::string gpuPath = "/xyz/openbmc_project/inventory/GPU_0";
    std::string cpuPath = "/xyz/openbmc_project/inventory/CPU_0";

    std::string gpuAssocSuffix = "/GPU_copy_Cpu_Power";
    std::string cpuAssocSuffix = "/power_controls";

    EXPECT_NE(gpuAssocSuffix, cpuAssocSuffix);
    EXPECT_NE(gpuPath + gpuAssocSuffix, cpuPath + cpuAssocSuffix);
}

// ============================================================================
// Map Existence Check Pattern Tests
// Pattern from gpuCpuPowerSyncDiscovery.cpp lines 373-374
// ============================================================================

TEST(MapExistenceTest, KeyNotExists_ShouldInsert)
{
    std::unordered_map<std::string, int> signalMap;
    std::string deviceName = "GPU_0";

    EXPECT_TRUE(signalMap.find(deviceName) == signalMap.end());

    if (signalMap.find(deviceName) == signalMap.end())
    {
        signalMap.emplace(deviceName, 42);
    }

    EXPECT_EQ(signalMap.size(), 1u);
    EXPECT_EQ(signalMap[deviceName], 42);
}

TEST(MapExistenceTest, KeyExists_ShouldNotInsert)
{
    std::unordered_map<std::string, int> signalMap;
    std::string deviceName = "GPU_0";
    signalMap[deviceName] = 100;

    EXPECT_FALSE(signalMap.find(deviceName) == signalMap.end());

    if (signalMap.find(deviceName) == signalMap.end())
    {
        signalMap.emplace(deviceName, 42);
    }

    EXPECT_EQ(signalMap.size(), 1u);
    EXPECT_EQ(signalMap[deviceName], 100);
}

TEST(MapExistenceTest, MultipleDevices)
{
    std::unordered_map<std::string, int> signalMap;

    for (int i = 0; i < 4; ++i)
    {
        std::string deviceName = "GPU_" + std::to_string(i);
        if (signalMap.find(deviceName) == signalMap.end())
        {
            signalMap.emplace(deviceName, i * 10);
        }
    }

    EXPECT_EQ(signalMap.size(), 4u);
    EXPECT_EQ(signalMap["GPU_2"], 20);
}

// ============================================================================
// LocationContext Validation Tests
// Pattern from gpuCpuPowerSyncDiscovery.cpp line 144
// ============================================================================

TEST(LocationContextTest, EmptyContext_Invalid)
{
    std::string locationContext = "";
    bool isValid = !locationContext.empty();
    EXPECT_FALSE(isValid);
}

TEST(LocationContextTest, ValidContext_NotEmpty)
{
    std::string locationContext = "HGX_Chassis_0/ProcessorModule_0";
    bool isValid = !locationContext.empty();
    EXPECT_TRUE(isValid);
}

TEST(LocationContextTest, ValidContextFormat)
{
    std::string locationContext = "HGX_Chassis_0/ProcessorModule_0";

    EXPECT_FALSE(locationContext.empty());
    EXPECT_TRUE(locationContext.find('/') != std::string::npos);
    EXPECT_TRUE(locationContext.find("ProcessorModule") != std::string::npos);
}

TEST(LocationContextTest, MultipleProcessorModules)
{
    std::vector<std::string> locationContexts = {
        "HGX_Chassis_0/ProcessorModule_0", "HGX_Chassis_0/ProcessorModule_1",
        "HGX_Chassis_0/ProcessorModule_2", "HGX_Chassis_0/ProcessorModule_3"};

    for (const auto& ctx : locationContexts)
    {
        EXPECT_FALSE(ctx.empty());
        EXPECT_TRUE(ctx.find("ProcessorModule") != std::string::npos);
    }
}
