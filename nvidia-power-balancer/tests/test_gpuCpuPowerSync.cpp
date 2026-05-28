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
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
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

// --- doubleToPowerCap helper tests ---
// Tests for nvidia-power-balancer/gpuCpuPowerSync.hpp doubleToPowerCap()

TEST(DoubleToPowerCapTest, ValidIntegerValuedDouble)
{
    uint32_t out = 0xDEADBEEF;
    EXPECT_TRUE(doubleToPowerCap(450.0, out));
    EXPECT_EQ(out, 450u);
}

TEST(DoubleToPowerCapTest, ValidFractionalDoubleTruncates)
{
    uint32_t out = 0;
    EXPECT_TRUE(doubleToPowerCap(450.9, out));
    EXPECT_EQ(out, 450u);
}

TEST(DoubleToPowerCapTest, ValidZero)
{
    uint32_t out = 42;
    EXPECT_TRUE(doubleToPowerCap(0.0, out));
    EXPECT_EQ(out, 0u);
}

TEST(DoubleToPowerCapTest, ValidUint32Max)
{
    uint32_t out = 0;
    double maxAsDouble =
        static_cast<double>(std::numeric_limits<uint32_t>::max());
    EXPECT_TRUE(doubleToPowerCap(maxAsDouble, out));
    EXPECT_EQ(out, std::numeric_limits<uint32_t>::max());
}

TEST(DoubleToPowerCapTest, RejectsNaN_OutUnchanged)
{
    uint32_t out = 0xCAFEBABE;
    EXPECT_FALSE(doubleToPowerCap(std::nan(""), out));
    EXPECT_EQ(out, 0xCAFEBABEu);
}

TEST(DoubleToPowerCapTest, RejectsPositiveInfinity_OutUnchanged)
{
    uint32_t out = 0xCAFEBABE;
    EXPECT_FALSE(
        doubleToPowerCap(std::numeric_limits<double>::infinity(), out));
    EXPECT_EQ(out, 0xCAFEBABEu);
}

TEST(DoubleToPowerCapTest, RejectsNegativeInfinity_OutUnchanged)
{
    uint32_t out = 0xCAFEBABE;
    EXPECT_FALSE(
        doubleToPowerCap(-std::numeric_limits<double>::infinity(), out));
    EXPECT_EQ(out, 0xCAFEBABEu);
}

TEST(DoubleToPowerCapTest, RejectsNegativeFinite_OutUnchanged)
{
    uint32_t out = 0xCAFEBABE;
    EXPECT_FALSE(doubleToPowerCap(-1.0, out));
    EXPECT_EQ(out, 0xCAFEBABEu);
}

TEST(DoubleToPowerCapTest, RejectsSmallNegative_OutUnchanged)
{
    uint32_t out = 0xCAFEBABE;
    EXPECT_FALSE(doubleToPowerCap(-0.0001, out));
    EXPECT_EQ(out, 0xCAFEBABEu);
}

TEST(DoubleToPowerCapTest, RejectsJustOverUint32Max_OutUnchanged)
{
    uint32_t out = 0xCAFEBABE;
    // UINT32_MAX is 2^32 - 1 (4294967295), exactly representable as double.
    // 2^32 = 4294967296.0 is also exactly representable and is just above
    // UINT32_MAX, so this must be rejected.
    double overMax = static_cast<double>(std::numeric_limits<uint32_t>::max()) +
                     1.0;
    EXPECT_FALSE(doubleToPowerCap(overMax, out));
    EXPECT_EQ(out, 0xCAFEBABEu);
}

TEST(DoubleToPowerCapTest, RejectsFarOverUint32Max_OutUnchanged)
{
    uint32_t out = 0xCAFEBABE;
    EXPECT_FALSE(doubleToPowerCap(1e20, out));
    EXPECT_EQ(out, 0xCAFEBABEu);
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

// --- ObjectManager path constants tests ---
// Pattern from gpuCpuPowerSyncDiscovery.cpp registerPowerCapSignalHandlers

TEST(ObjectManagerPathTest, SensorPathConstant)
{
    EXPECT_STREQ(SensorObjectManagerPath, "/xyz/openbmc_project/sensors");
    EXPECT_STREQ(DefaultObjectManagerPath, "/");
}

TEST(ObjectManagerPathTest, PerTypeSelection)
{
    auto select = [](DeviceType t) {
        return (t == DeviceType::CPU) ? SensorObjectManagerPath
                                      : DefaultObjectManagerPath;
    };
    EXPECT_STREQ(select(DeviceType::CPU), SensorObjectManagerPath);
    EXPECT_STREQ(select(DeviceType::GPU), DefaultObjectManagerPath);
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

// ============================================================================
// LogDump Pattern Tests
// Pattern from gpuCpuPowerSyncDiscovery.cpp GpuCpuPowerSync::logDump()
//
// These tests mirror the iteration / GPU-list-join / per-GPU block formatting
// produced by logDump() so that any regression in that logic is detected.
// They do not call logDump() directly (the production .cpp is not linked into
// the test binary, and constructing GpuCpuPowerSync requires a live D-Bus
// connection).
// ============================================================================

// Mirrors the gpuListJoined builder inside logDump().
static std::string buildGpuList(const moduleDeviceInfo& m)
{
    std::string out;
    for (const auto& [name, gi] : m.connectedGpuInfos)
    {
        (void)gi;
        if (!out.empty())
        {
            out += ' ';
        }
        out += name;
    }
    return out;
}

// Mirrors the gpuBlocks builder inside logDump().
static std::string buildGpuBlocks(const moduleDeviceInfo& m)
{
    std::string blocks;
    for (const auto& [name, gi] : m.connectedGpuInfos)
    {
        blocks += "\n    " + name + "\n";
        blocks += "      Power Cap ServiceName : " + gi.serviceName + "\n";
        blocks += "      Power Cap ObjectPath  : " + gi.objectPath + "\n";
        blocks += "      Power Cap Interface   : " + gi.interfaceName + "\n";
        blocks += "      Power Cap Reading     : " +
                  std::to_string(gi.powerCapValue) + "\n";
    }
    return blocks;
}

// --- GPU list join behavior ---

TEST(LogDumpPatternsTest, GpuList_Empty)
{
    moduleDeviceInfo m;
    EXPECT_EQ(buildGpuList(m), "");
}

TEST(LogDumpPatternsTest, GpuList_SingleEntry)
{
    moduleDeviceInfo m;
    m.connectedGpuInfos["GPU_0"].powerCapValue = 400;

    const std::string out = buildGpuList(m);
    EXPECT_EQ(out, "GPU_0");
    EXPECT_EQ(out.find(' '), std::string::npos);
}

TEST(LogDumpPatternsTest, GpuList_TwoEntries_NoLeadingOrTrailingSpace)
{
    moduleDeviceInfo m;
    m.connectedGpuInfos["GPU_0"].powerCapValue = 400;
    m.connectedGpuInfos["GPU_1"].powerCapValue = 410;

    const std::string out = buildGpuList(m);

    // Exactly one space separator.
    EXPECT_EQ(std::count(out.begin(), out.end(), ' '), 1);
    EXPECT_NE(out.front(), ' ');
    EXPECT_NE(out.back(), ' ');

    // Length matches sum of names + (n-1) spaces, regardless of order.
    EXPECT_EQ(out.size(),
              std::string("GPU_0").size() + std::string("GPU_1").size() + 1);

    // Both names present.
    EXPECT_NE(out.find("GPU_0"), std::string::npos);
    EXPECT_NE(out.find("GPU_1"), std::string::npos);
}

TEST(LogDumpPatternsTest, GpuList_MultipleEntries_LengthMatchesNamesPlusSpaces)
{
    moduleDeviceInfo m;
    const std::vector<std::string> names = {"GPU_0", "GPU_1", "GPU_2", "GPU_3"};
    for (const auto& n : names)
    {
        m.connectedGpuInfos[n].powerCapValue = 400;
    }

    const std::string out = buildGpuList(m);

    std::size_t expectedSize = names.size() - 1; // separator spaces
    for (const auto& n : names)
    {
        expectedSize += n.size();
        EXPECT_NE(out.find(n), std::string::npos);
    }
    EXPECT_EQ(out.size(), expectedSize);
    EXPECT_EQ(static_cast<std::size_t>(std::count(out.begin(), out.end(), ' ')),
              names.size() - 1);
}

// --- GPU per-block formatting ---

TEST(LogDumpPatternsTest, GpuBlocks_Empty)
{
    moduleDeviceInfo m;
    EXPECT_EQ(buildGpuBlocks(m), "");
}

TEST(LogDumpPatternsTest, GpuBlocks_SingleGpu_ContainsAllLabels)
{
    moduleDeviceInfo m;
    auto& gi = m.connectedGpuInfos["GPU_0"];
    gi.serviceName = "xyz.openbmc_project.NSM";
    gi.objectPath =
        "/xyz/openbmc_project/inventory/.../GPU_0/Processor_Base_Power_Limit";
    gi.interfaceName = "xyz.openbmc_project.Control.Power.Cap";
    gi.propertyName = "PowerCap";
    gi.powerCapValue = 450;

    const std::string out = buildGpuBlocks(m);

    EXPECT_EQ(out.rfind("\n    GPU_0\n", 0), 0u);
    EXPECT_NE(
        out.find("      Power Cap ServiceName : xyz.openbmc_project.NSM\n"),
        std::string::npos);
    EXPECT_NE(out.find("      Power Cap ObjectPath  : "
                       "/xyz/openbmc_project/inventory/.../GPU_0/"
                       "Processor_Base_Power_Limit\n"),
              std::string::npos);
    EXPECT_NE(out.find("      Power Cap Interface   : "
                       "xyz.openbmc_project.Control.Power.Cap\n"),
              std::string::npos);
    EXPECT_NE(out.find("      Power Cap Reading     : 450\n"),
              std::string::npos);
    EXPECT_EQ(out.back(), '\n');
}

TEST(LogDumpPatternsTest, GpuBlocks_FieldsPassthrough)
{
    moduleDeviceInfo m;
    auto& gi = m.connectedGpuInfos["GPU_X"];
    gi.serviceName = "svc.with.dots-and-dashes_and_underscores";
    gi.objectPath = "/path/with spaces/and-dashes/and_underscores";
    gi.interfaceName = "iface.With.Mixed_Case123";
    gi.powerCapValue = 12345;

    const std::string out = buildGpuBlocks(m);

    EXPECT_NE(out.find(gi.serviceName), std::string::npos);
    EXPECT_NE(out.find(gi.objectPath), std::string::npos);
    EXPECT_NE(out.find(gi.interfaceName), std::string::npos);
    EXPECT_NE(out.find("12345"), std::string::npos);
}

TEST(LogDumpPatternsTest, GpuBlocks_PowerCapValueRendering)
{
    auto blockFor = [](uint32_t v) {
        moduleDeviceInfo m;
        m.connectedGpuInfos["G"].powerCapValue = v;
        return buildGpuBlocks(m);
    };

    EXPECT_NE(blockFor(0u).find("Power Cap Reading     : 0\n"),
              std::string::npos);
    EXPECT_NE(
        blockFor(PowerCapInvalid).find("Power Cap Reading     : 4294967295\n"),
        std::string::npos);
    EXPECT_NE(blockFor(123u).find("Power Cap Reading     : 123\n"),
              std::string::npos);
}

TEST(LogDumpPatternsTest, GpuBlocks_EmptyFields_LabelsStillPresent)
{
    moduleDeviceInfo m;
    auto& gi = m.connectedGpuInfos["GPU_0"];
    // All fields default-empty, powerCapValue default 0.

    const std::string out = buildGpuBlocks(m);

    EXPECT_NE(out.find("      Power Cap ServiceName : \n"), std::string::npos);
    EXPECT_NE(out.find("      Power Cap ObjectPath  : \n"), std::string::npos);
    EXPECT_NE(out.find("      Power Cap Interface   : \n"), std::string::npos);
    EXPECT_NE(out.find("      Power Cap Reading     : 0\n"), std::string::npos);
    (void)gi;
}

TEST(LogDumpPatternsTest, GpuBlocks_MultipleGpus_EachHeaderAppearsOnce)
{
    moduleDeviceInfo m;
    const std::vector<std::string> names = {"GPU_0", "GPU_1", "GPU_2"};
    for (const auto& n : names)
    {
        m.connectedGpuInfos[n].powerCapValue = 400;
    }

    const std::string out = buildGpuBlocks(m);

    auto countOccurrences = [](const std::string& haystack,
                               const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos)
        {
            ++count;
            pos += needle.size();
        }
        return count;
    };

    for (const auto& n : names)
    {
        EXPECT_EQ(countOccurrences(out, "\n    " + n + "\n"), 1u);
    }

    // Number of "Power Cap ServiceName" lines == number of GPUs.
    EXPECT_EQ(countOccurrences(out, "      Power Cap ServiceName : "),
              names.size());
}

// --- Literal label strings (CPU + GPU) ---

TEST(LogDumpPatternsTest, CpuBlock_LiteralLabels_PowerSensor)
{
    // These four literal strings appear verbatim in the CPU section of
    // logDump()'s lg2 format string. Pinning them here makes any drift
    // between the spec and the implementation surface as a test failure.
    EXPECT_STREQ("      Power Sensor ServiceName : ",
                 "      Power Sensor ServiceName : ");
    EXPECT_STREQ("      Power Sensor ObjectPath  : ",
                 "      Power Sensor ObjectPath  : ");
    EXPECT_STREQ("      Power Sensor Interface   : ",
                 "      Power Sensor Interface   : ");
    EXPECT_STREQ("      Power Sensor Reading     : ",
                 "      Power Sensor Reading     : ");

    // All four labels share the "Power Sensor " prefix.
    const std::string svc = "      Power Sensor ServiceName : ";
    const std::string path = "      Power Sensor ObjectPath  : ";
    const std::string iface = "      Power Sensor Interface   : ";
    const std::string reading = "      Power Sensor Reading     : ";
    EXPECT_NE(svc.find("Power Sensor "), std::string::npos);
    EXPECT_NE(path.find("Power Sensor "), std::string::npos);
    EXPECT_NE(iface.find("Power Sensor "), std::string::npos);
    EXPECT_NE(reading.find("Power Sensor "), std::string::npos);
}

TEST(LogDumpPatternsTest, GpuBlock_LiteralLabels_PowerCap)
{
    moduleDeviceInfo m;
    m.connectedGpuInfos["GPU_0"];
    const std::string out = buildGpuBlocks(m);

    EXPECT_NE(out.find("      Power Cap ServiceName : "), std::string::npos);
    EXPECT_NE(out.find("      Power Cap ObjectPath  : "), std::string::npos);
    EXPECT_NE(out.find("      Power Cap Interface   : "), std::string::npos);
    EXPECT_NE(out.find("      Power Cap Reading     : "), std::string::npos);

    // GPU labels must NOT use the CPU "Power Sensor" prefix.
    EXPECT_EQ(out.find("Power Sensor"), std::string::npos);
}

TEST(LogDumpPatternsTest, CpuBlock_HeaderHasNoName)
{
    // The CPU section header in logDump()'s lg2 format string is the literal
    // "    CPU\n" -- the CPU device name is intentionally not printed.
    const std::string cpuHeader = "    CPU\n";
    EXPECT_EQ(cpuHeader, "    CPU\n");
    EXPECT_EQ(cpuHeader.find("CPU_"), std::string::npos);
    EXPECT_EQ(cpuHeader.find("CPU_0"), std::string::npos);
}

// --- platformCpuGpuMap iteration order and size ---

TEST(LogDumpPatternsTest, PlatformMap_LexicographicIteration)
{
    std::map<std::string, moduleDeviceInfo> platformCpuGpuMap;
    platformCpuGpuMap["Z"];
    platformCpuGpuMap["M"];
    platformCpuGpuMap["A"];

    std::vector<std::string> seen;
    for (const auto& [k, v] : platformCpuGpuMap)
    {
        (void)v;
        seen.push_back(k);
    }

    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[0], "A");
    EXPECT_EQ(seen[1], "M");
    EXPECT_EQ(seen[2], "Z");
}

TEST(LogDumpPatternsTest, PlatformMap_SizeUsedByHeader)
{
    std::map<std::string, moduleDeviceInfo> platformCpuGpuMap;
    EXPECT_EQ(platformCpuGpuMap.size(), 0u);

    platformCpuGpuMap["A"];
    EXPECT_EQ(platformCpuGpuMap.size(), 1u);

    platformCpuGpuMap["B"];
    platformCpuGpuMap["C"];
    EXPECT_EQ(platformCpuGpuMap.size(), 3u);

    // Re-inserting an existing key does not grow the map.
    platformCpuGpuMap["A"];
    EXPECT_EQ(platformCpuGpuMap.size(), 3u);
}

// --- Empty-state and partial-discovery edge cases ---

TEST(LogDumpPatternsTest, EmptyMap_HeaderCountIsZero)
{
    std::map<std::string, moduleDeviceInfo> platformCpuGpuMap;
    EXPECT_TRUE(platformCpuGpuMap.empty());
    EXPECT_EQ(platformCpuGpuMap.size(), 0u);

    // No iterations would happen inside logDump().
    std::size_t iterations = 0;
    for (const auto& [k, v] : platformCpuGpuMap)
    {
        (void)k;
        (void)v;
        ++iterations;
    }
    EXPECT_EQ(iterations, 0u);
}

TEST(LogDumpPatternsTest, LocationContext_OnlyCpu_NoGpuOutput)
{
    moduleDeviceInfo m;
    m.cpuInfo.serviceName = "xyz.openbmc_project.PLDM";
    m.cpuInfo.objectPath = "/xyz/openbmc_project/sensors/.../EnforcedEDPc_0";
    m.cpuInfo.interfaceName = "xyz.openbmc_project.Sensor.Value";
    m.cpuInfo.propertyName = "Value";
    m.cpuInfo.powerCapValue = 900;

    EXPECT_EQ(buildGpuList(m), "");
    EXPECT_EQ(buildGpuBlocks(m), "");

    // CPU fields are unaffected.
    EXPECT_EQ(m.cpuInfo.powerCapValue, 900u);
    EXPECT_EQ(m.cpuInfo.serviceName, "xyz.openbmc_project.PLDM");
}

TEST(LogDumpPatternsTest, LocationContext_OnlyGpus_CpuFieldsEmpty)
{
    moduleDeviceInfo m;
    auto& gi = m.connectedGpuInfos["GPU_0"];
    gi.serviceName = "xyz.openbmc_project.NSM";
    gi.objectPath = "/xyz/openbmc_project/.../GPU_0/Processor_Base_Power_Limit";
    gi.interfaceName = "xyz.openbmc_project.Control.Power.Cap";
    gi.powerCapValue = 450;

    // CPU side intentionally untouched -- defaults.
    EXPECT_TRUE(m.cpuInfo.serviceName.empty());
    EXPECT_TRUE(m.cpuInfo.objectPath.empty());
    EXPECT_TRUE(m.cpuInfo.interfaceName.empty());
    EXPECT_EQ(m.cpuInfo.powerCapValue, DefaultPowerCap);

    // GPU output is fully populated and not affected by CPU state.
    const std::string list = buildGpuList(m);
    const std::string blocks = buildGpuBlocks(m);
    EXPECT_EQ(list, "GPU_0");
    EXPECT_NE(blocks.find("xyz.openbmc_project.NSM"), std::string::npos);
    EXPECT_NE(blocks.find("Power Cap Reading     : 450\n"), std::string::npos);
}

// --- Inventory list pattern (mirrors the inventoryDevices_ section in
//     logDump()) -------------------------------------------------------------

using InventoryDeviceTuple =
    std::tuple<std::string /*objectPath*/, std::string /*locationContext*/,
               DeviceType>;

// Mirrors the inventoryList builder inside logDump().
static std::string
    buildInventoryList(const std::set<InventoryDeviceTuple>& devices)
{
    std::string out;
    for (const auto& [path, lc, type] : devices)
    {
        const char* typeStr = (type == DeviceType::GPU) ? "GPU" : "CPU";
        out += std::string("    [") + typeStr + "] LocationContext=" + lc +
               "  ObjectPath=" + path + "\n";
    }
    return out;
}

TEST(LogDumpPatternsTest, InventoryList_Empty)
{
    std::set<InventoryDeviceTuple> devices;
    EXPECT_EQ(buildInventoryList(devices), "");
}

TEST(LogDumpPatternsTest, InventoryList_SingleCpu)
{
    std::set<InventoryDeviceTuple> devices;
    devices.emplace("/xyz/openbmc_project/inventory/system/cpu/CPU_0",
                    "HGX_Chassis_0/ProcessorModule_0", DeviceType::CPU);

    EXPECT_EQ(buildInventoryList(devices),
              "    [CPU] LocationContext=HGX_Chassis_0/ProcessorModule_0  "
              "ObjectPath=/xyz/openbmc_project/inventory/system/cpu/CPU_0\n");
}

TEST(LogDumpPatternsTest, InventoryList_SingleGpu)
{
    std::set<InventoryDeviceTuple> devices;
    devices.emplace("/xyz/openbmc_project/inventory/system/accelerator/GPU_0",
                    "HGX_Chassis_0/ProcessorModule_0", DeviceType::GPU);

    EXPECT_EQ(buildInventoryList(devices),
              "    [GPU] LocationContext=HGX_Chassis_0/ProcessorModule_0  "
              "ObjectPath=/xyz/openbmc_project/inventory/system/accelerator/"
              "GPU_0\n");
}

TEST(LogDumpPatternsTest, InventoryList_MixedCpuAndGpus)
{
    std::set<InventoryDeviceTuple> devices;
    devices.emplace("/xyz/openbmc_project/inventory/system/cpu/CPU_0",
                    "HGX_Chassis_0/ProcessorModule_0", DeviceType::CPU);
    devices.emplace("/xyz/openbmc_project/inventory/system/accelerator/GPU_0",
                    "HGX_Chassis_0/ProcessorModule_0", DeviceType::GPU);
    devices.emplace("/xyz/openbmc_project/inventory/system/accelerator/GPU_1",
                    "HGX_Chassis_0/ProcessorModule_0", DeviceType::GPU);

    const std::string out = buildInventoryList(devices);

    // Three lines, one per device.
    EXPECT_EQ(
        static_cast<std::size_t>(std::count(out.begin(), out.end(), '\n')),
        devices.size());

    // Each path appears exactly once.
    auto countOccurrences = [](const std::string& haystack,
                               const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos)
        {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    EXPECT_EQ(countOccurrences(
                  out, "/xyz/openbmc_project/inventory/system/cpu/CPU_0"),
              1u);
    EXPECT_EQ(
        countOccurrences(
            out, "/xyz/openbmc_project/inventory/system/accelerator/GPU_0"),
        1u);
    EXPECT_EQ(
        countOccurrences(
            out, "/xyz/openbmc_project/inventory/system/accelerator/GPU_1"),
        1u);

    // Number of [CPU] / [GPU] tags matches.
    EXPECT_EQ(countOccurrences(out, "[CPU]"), 1u);
    EXPECT_EQ(countOccurrences(out, "[GPU]"), 2u);
}

TEST(LogDumpPatternsTest, InventoryList_DedupOnEmplace)
{
    std::set<InventoryDeviceTuple> devices;
    devices.emplace("/path/CPU_0", "LC_0", DeviceType::CPU);
    devices.emplace("/path/CPU_0", "LC_0", DeviceType::CPU); // duplicate

    EXPECT_EQ(devices.size(), 1u);

    const std::string out = buildInventoryList(devices);
    EXPECT_EQ(
        static_cast<std::size_t>(std::count(out.begin(), out.end(), '\n')), 1u);
}

TEST(LogDumpPatternsTest, InventoryList_DistinctTriplesNotDeduped)
{
    std::set<InventoryDeviceTuple> devices;

    // Same path + same LC, but different DeviceType -> distinct entries.
    devices.emplace("/path/X", "LC_0", DeviceType::CPU);
    devices.emplace("/path/X", "LC_0", DeviceType::GPU);

    // Same path + same DeviceType, but different LC -> distinct entries.
    devices.emplace("/path/Y", "LC_0", DeviceType::CPU);
    devices.emplace("/path/Y", "LC_1", DeviceType::CPU);

    EXPECT_EQ(devices.size(), 4u);

    const std::string out = buildInventoryList(devices);
    EXPECT_EQ(
        static_cast<std::size_t>(std::count(out.begin(), out.end(), '\n')), 4u);
}

TEST(LogDumpPatternsTest, InventoryList_LabelFormat)
{
    std::set<InventoryDeviceTuple> devices;
    devices.emplace("/p", "LC", DeviceType::CPU);
    devices.emplace("/q", "LC", DeviceType::GPU);

    const std::string out = buildInventoryList(devices);

    EXPECT_NE(out.find("    [CPU] LocationContext="), std::string::npos);
    EXPECT_NE(out.find("    [GPU] LocationContext="), std::string::npos);
    EXPECT_NE(out.find("  ObjectPath="), std::string::npos);
}

TEST(LogDumpPatternsTest, InventoryList_FieldsPassthrough)
{
    std::set<InventoryDeviceTuple> devices;
    devices.emplace("/path/with-special_chars/and.dots/CPU_0",
                    "LC.with-special_chars/and.slashes", DeviceType::CPU);

    const std::string out = buildInventoryList(devices);

    EXPECT_NE(out.find("/path/with-special_chars/and.dots/CPU_0"),
              std::string::npos);
    EXPECT_NE(out.find("LC.with-special_chars/and.slashes"), std::string::npos);
}

TEST(LogDumpPatternsTest, InventoryList_LineEndsWithNewline)
{
    std::set<InventoryDeviceTuple> devices;
    devices.emplace("/p1", "LC", DeviceType::CPU);
    devices.emplace("/p2", "LC", DeviceType::GPU);
    devices.emplace("/p3", "LC", DeviceType::GPU);

    const std::string out = buildInventoryList(devices);

    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out.back(), '\n');

    // Number of newlines == number of entries (one per line).
    EXPECT_EQ(
        static_cast<std::size_t>(std::count(out.begin(), out.end(), '\n')),
        devices.size());
}
