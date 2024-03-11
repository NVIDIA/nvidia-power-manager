#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>

#include <sdbusplus/test/sdbus_mock.hpp>
#include "config.h"
#include "cpld_util.hpp"
#include "cpld.hpp"

#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrEq;

static void sdbusMockExpectPropertyChanged(sdbusplus::SdBusMock & sdbus_mock,
                                                std::string path, std::string intf,
                                                std::string expectedName) {
EXPECT_CALL(sdbus_mock,
        sd_bus_emit_properties_changed_strv(IsNull(), StrEq(path),
                                            StrEq(intf), NotNull()))
    .WillOnce(Invoke(
        [=](sd_bus*, const char*, const char*, const char** names) {
            EXPECT_STREQ(expectedName.c_str(), names[0]);
            return 0;
        }));
}

static void sdbusMockExpectPropertyChangeMultiple(sdbusplus::SdBusMock & sdbus_mock,
                                                std::string path, std::string intf,
                                                std::vector<std::string> &expectedNames) {
    EXPECT_CALL(sdbus_mock,
        sd_bus_emit_properties_changed_strv(IsNull(), StrEq(path),
                                            StrEq(intf), NotNull()))
        .WillRepeatedly(Invoke(
            [=](sd_bus*, const char*, const char*, const char** names) {
                if (std::none_of(expectedNames.begin(), expectedNames.end(),
                        [names](const std::string s) {return s == names[0];})) {
                    ADD_FAILURE();
                }
                return 0;
            }));
}
/* ensure that the dbus entries we expect get filled in */
TEST(CpldTest, CpldTestDbus)
{
    std::string path = "/xyz/openbmc_project/inventory/system/chassis/motherboard/CPLD0";
    std::string swpath = "/xyz/openbmc_project/software/CPLD0";
    sdbusplus::SdBusMock sdbus_mock;
    auto bus_mock = sdbusplus::get_mocked_new(&sdbus_mock);
    std::vector<std::string> itemNames = {"Present", "PrettyName"};
    std::vector<std::string> assetNames = {"Manufacturer", "Model", "PartNumber", "SerialNumber"};
    std::vector<std::string> versionNames = {"Purpose", "SoftwareId",
                                             "Version"};
    std::vector<std::string> operationalStatusNames = {"Functional", "State"};

    sdbusMockExpectPropertyChangeMultiple(sdbus_mock, path, "xyz.openbmc_project.Inventory.Item", itemNames);
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, path, "xyz.openbmc_project.Inventory.Decorator.Asset",
        assetNames);
    sdbusMockExpectPropertyChangeMultiple(sdbus_mock, path, "xyz.openbmc_project.State.Decorator.OperationalStatus", operationalStatusNames);
    sdbusMockExpectPropertyChanged(sdbus_mock, path, "xyz.openbmc_project.Inventory.Item.Chassis", "Type");
    sdbusMockExpectPropertyChangeMultiple(
        sdbus_mock, swpath, "xyz.openbmc_project.Software.Version",
        versionNames);
    sdbusMockExpectPropertyChanged(
        sdbus_mock, swpath, "xyz.openbmc_project.Association.Definitions",
        "Associations");

    nvidia::cpld::device::Cpld(bus_mock, path, 2, 0x3c, "name", "", "");
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
