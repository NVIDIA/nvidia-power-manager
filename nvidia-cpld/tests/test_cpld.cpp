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

#include "config.h"

#include "cpld.hpp"
#include "cpld_util.hpp"

#include <sdbusplus/test/sdbus_mock.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrEq;

static void sdbusMockExpectPropertyChanged(sdbusplus::SdBusMock& sdbus_mock,
                                           std::string path, std::string intf,
                                           std::string expectedName)
{
    EXPECT_CALL(sdbus_mock, sd_bus_emit_properties_changed_strv(
                                IsNull(), StrEq(path), StrEq(intf), NotNull()))
        .WillOnce(
            Invoke([=](sd_bus*, const char*, const char*, const char** names) {
        EXPECT_STREQ(expectedName.c_str(), names[0]);
        return 0;
    }));
}

static void sdbusMockExpectPropertyChangeMultiple(
    sdbusplus::SdBusMock& sdbus_mock, std::string path, std::string intf,
    std::vector<std::string>& expectedNames)
{
    EXPECT_CALL(sdbus_mock, sd_bus_emit_properties_changed_strv(
                                IsNull(), StrEq(path), StrEq(intf), NotNull()))
        .WillRepeatedly(
            Invoke([=](sd_bus*, const char*, const char*, const char** names) {
        if (std::none_of(
                expectedNames.begin(), expectedNames.end(),
                [names](const std::string s) { return s == names[0]; }))
        {
            ADD_FAILURE();
        }
        return 0;
    }));
}
/* ensure that the dbus entries we expect get filled in */
TEST(CpldTest, CpldTestDbus)
{
    std::string path =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/CPLD0";
    std::string swpath = "/xyz/openbmc_project/software/CPLD0";
    sdbusplus::SdBusMock sdbus_mock;
    auto bus_mock = sdbusplus::get_mocked_new(&sdbus_mock);
    std::vector<std::string> itemNames = {"Present", "PrettyName"};
    std::vector<std::string> assetNames = {"Manufacturer", "Model",
                                           "PartNumber", "SerialNumber"};
    std::vector<std::string> versionNames = {"Purpose", "SoftwareId",
                                             "Version"};
    std::vector<std::string> operationalStatusNames = {"Functional", "State"};

    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, path, "xyz.openbmc_project.Inventory.Item", itemNames);
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, path, "xyz.openbmc_project.Inventory.Decorator.Asset",
        assetNames);
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, path,
        "xyz.openbmc_project.State.Decorator.OperationalStatus",
        operationalStatusNames);
    sdbusMockExpectPropertyChanged(
        sdbus_mock, path, "xyz.openbmc_project.Inventory.Item.Chassis", "Type");
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, swpath, "xyz.openbmc_project.Software.Version",
        versionNames);
    sdbusMockExpectPropertyChanged(
        sdbus_mock, swpath, "xyz.openbmc_project.Association.Definitions",
        "Associations");

    nvidia::cpld::device::Cpld(bus_mock, path, 2, 0x3c, "name", "", "", "", "");
}

/* I2C bus and device address used by the parent chassis association tests */
constexpr uint8_t testBusId = 2;
constexpr uint8_t testDeviceAddress = 0x3c;

/* parent_chassis follows ChassisAssociationEndpoint unless overridden */
TEST(CpldTest, ParentChassisDefaultsToAssociationEndpoint)
{
    std::string path =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/CPLD0";
    sdbusplus::SdBusMock sdbus_mock;
    auto bus_mock = sdbusplus::get_mocked_new(&sdbus_mock);

    EXPECT_CALL(sdbus_mock, sd_bus_emit_properties_changed_strv(_, _, _, _))
        .WillRepeatedly(Return(0));
    sdbusMockExpectPropertyChanged(
        sdbus_mock, path, "xyz.openbmc_project.Association.Definitions",
        "Associations");

    const std::string endpoint = "/xyz/openbmc_project/inventory/chassis0";
    nvidia::cpld::device::Cpld cpld(bus_mock, path, testBusId,
                                    testDeviceAddress, "name", "", "", endpoint,
                                    "");

    const nvidia::cpld::device::AssociationList expected = {
        {"parent_chassis", "inventory", endpoint}};
    EXPECT_EQ(cpld.associations(), expected);
}

TEST(CpldTest, ParentChassisOverridesAssociationEndpoint)
{
    std::string path =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/CPLD0";
    sdbusplus::SdBusMock sdbus_mock;
    auto bus_mock = sdbusplus::get_mocked_new(&sdbus_mock);

    EXPECT_CALL(sdbus_mock, sd_bus_emit_properties_changed_strv(_, _, _, _))
        .WillRepeatedly(Return(0));
    sdbusMockExpectPropertyChanged(
        sdbus_mock, path, "xyz.openbmc_project.Association.Definitions",
        "Associations");

    const std::string firmwareEndpoint = path;
    const std::string parentEndpoint =
        "/xyz/openbmc_project/inventory/chassis0";
    nvidia::cpld::device::Cpld cpld(bus_mock, path, testBusId,
                                    testDeviceAddress, "name", "", "",
                                    firmwareEndpoint, parentEndpoint, "");

    const nvidia::cpld::device::AssociationList expected = {
        {"parent_chassis", "inventory", parentEndpoint}};
    EXPECT_EQ(cpld.associations(), expected);
}

/* an empty parent chassis must not create a parent_chassis association */
TEST(CpldTest, EmptyParentChassisCreatesNoAssociation)
{
    std::string path =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/CPLD0";
    sdbusplus::SdBusMock sdbus_mock;
    auto bus_mock = sdbusplus::get_mocked_new(&sdbus_mock);

    EXPECT_CALL(sdbus_mock, sd_bus_emit_properties_changed_strv(_, _, _, _))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(sdbus_mock, sd_bus_emit_properties_changed_strv(
                                IsNull(), StrEq(path),
                                StrEq("xyz.openbmc_project.Association."
                                      "Definitions"),
                                _))
        .Times(0);

    nvidia::cpld::device::Cpld cpld(bus_mock, path, testBusId,
                                    testDeviceAddress, "name", "", "", path, "",
                                    "");

    EXPECT_TRUE(cpld.associations().empty());
}

/* test JSON load */
TEST(CpldTest, JsonFiles)
{
    std::string tempname_str = "/tmp/cpldUtilTestFile.json";
    std::string validJsonInput = R"(
{
    "CPLD": [
        {
            "Index" : "0",
            "Bus" : "2",
            "Address" : "3c"
        },
        {
            "Index" : "1",
            "Bus" : "2",
            "Address" : "3c"
        }
    ]
})";

    auto r = nvidia::cpld::common::loadJSONFile(tempname_str.c_str());
    EXPECT_EQ(r, nullptr);

    std::string tempFileCmd = "touch " + tempname_str;
    auto ec = system(tempFileCmd.c_str());

    r = nvidia::cpld::common::loadJSONFile(tempname_str.c_str());
    EXPECT_EQ(r, nullptr);

    std::ofstream outfile(tempname_str);
    outfile << validJsonInput;
    outfile.close();

    r = nvidia::cpld::common::loadJSONFile(tempname_str.c_str());
    EXPECT_NE(r, nullptr);

    tempFileCmd = "rm " + tempname_str;
    ec = system(tempFileCmd.c_str());
    EXPECT_EQ(ec, 0);
}
