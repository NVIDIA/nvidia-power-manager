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

#include "power_supply.hpp"
#include "psu_util.hpp"

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
TEST(PsuTest, PsuTestDbus)
{
    std::string path = "/xyz/openbmc_project/inventory/system/chassis/"
                       "motherboard/powersupply/PSU0";
    std::string swpath = "/xyz/openbmc_project/software/PSU0";
    std::string sensorpath =
        "/xyz/openbmc_project/sensors/system/chassis/motherboard/PSU0";
    sdbusplus::SdBusMock sdbus_mock;
    auto bus_mock = sdbusplus::get_mocked_new(&sdbus_mock);
    std::vector<std::string> itemNames = {"Present", "PrettyName"};
    std::vector<std::string> assetNames = {"Manufacturer", "Model",
                                           "PartNumber", "SerialNumber"};
    std::vector<std::string> versionNames = {"Purpose", "SoftwareId",
                                             "Version"};

    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, path, "xyz.openbmc_project.Inventory.Item", itemNames);
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, path, "xyz.openbmc_project.Inventory.Decorator.Asset",
        assetNames);
    sdbusMockExpectPropertyChanged(
        sdbus_mock, path,
        "xyz.openbmc_project.State.Decorator.OperationalStatus", "Functional");
    sdbusMockExpectPropertyChanged(
        sdbus_mock, path, "xyz.openbmc_project.State.Decorator.PowerState",
        "PowerState");
    sdbusMockExpectPropertyChanged(
        sdbus_mock, path, "xyz.openbmc_project.Association.Definitions",
        "Associations");
    sdbusMockExpectPropertyChangeMultiple(sdbus_mock, sensorpath,
                                          "xyz.openbmc_project.Inventory.Item",
                                          itemNames);
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, sensorpath, "xyz.openbmc_project.Inventory.Decorator.Asset",
        assetNames);
    sdbusMockExpectPropertyChanged(
        sdbus_mock, sensorpath,
        "xyz.openbmc_project.State.Decorator.OperationalStatus", "Functional");
    sdbusMockExpectPropertyChanged(
        sdbus_mock, sensorpath,
        "xyz.openbmc_project.State.Decorator.PowerState", "PowerState");
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, swpath, "xyz.openbmc_project.Software.Version",
        versionNames);
    sdbusMockExpectPropertyChanged(
        sdbus_mock, swpath, "xyz.openbmc_project.Association.Definitions",
        "Associations");

    nvidia::power::psu::PowerSupply(
        bus_mock, path, "echo", "name",
        "/xyz/openbmc_project/inventory/system/board/Luna_Motherboard");
}

/* test JSON load */
TEST(PsuUtil, JsonFiles)
{
    std::string tempname_str = "/tmp/psuUtilTestFile.json";
    std::string validJsonInput = R"(
{
    "CommandName": "psui2ccmd.sh",
    "PowerSupplies": [
        {
            "Index": "0",
            "ChassisAssociationEndpoint": "/xyz/openbmc_project/inventory/system/board/Luna_Motherboard"
        },
        {
            "Index": "1",
            "ChassisAssociationEndpoint": "/xyz/openbmc_project/inventory/system/board/Luna_Motherboard"
        },
        {
            "Index": "2",
            "ChassisAssociationEndpoint": "/xyz/openbmc_project/inventory/system/board/Luna_Motherboard"
        },
        {
            "Index": "3",
            "ChassisAssociationEndpoint": "/xyz/openbmc_project/inventory/system/board/Luna_Motherboard"
        },
        {
            "Index": "4",
            "ChassisAssociationEndpoint": "/xyz/openbmc_project/inventory/system/board/Luna_Motherboard"
        },
        {
            "Index": "5",
            "ChassisAssociationEndpoint": "/xyz/openbmc_project/inventory/system/board/Luna_Motherboard"
        }
    ]
})";

    auto r = nvidia::power::common::loadJSONFile(tempname_str.c_str());
    EXPECT_EQ(r, nullptr);

    std::string tempFileCmd = "touch " + tempname_str;
    auto ec = system(tempFileCmd.c_str());

    r = nvidia::power::common::loadJSONFile(tempname_str.c_str());
    EXPECT_EQ(r, nullptr);

    std::ofstream outfile(tempname_str);
    outfile << validJsonInput;
    outfile.close();

    r = nvidia::power::common::loadJSONFile(tempname_str.c_str());
    EXPECT_NE(r, nullptr);

    tempFileCmd = "rm " + tempname_str;
    ec = system(tempFileCmd.c_str());
    EXPECT_EQ(ec, 0);
}
