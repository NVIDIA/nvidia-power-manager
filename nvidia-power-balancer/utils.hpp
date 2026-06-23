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

#pragma once

#include <boost/system/error_code.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <functional>

namespace utils
{

using ObjectPath = std::string;
using ServiceName = std::string;
using InterfaceName = std::string;
using PropertyName = std::string;
using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, std::vector<uint8_t>,
                 std::vector<std::string>>;
using InterfaceList = std::vector<std::string>;
using PropertyMap = std::map<PropertyName, PropertyValue>;
using InterfaceMap = std::map<InterfaceName, PropertyMap>;
using ServiceMap = std::map<ServiceName, InterfaceList>;
using GetSubTreeResponse = std::map<ObjectPath, ServiceMap>;
using ManagedObjectsResponse = std::map<sdbusplus::object_path, InterfaceMap>;

/**
 * @brief Asynchronously get a D-Bus property
 *
 * @param bus         - Shared pointer to async D-Bus connection
 * @param serviceName - D-Bus service name (e.g.,
 * "xyz.openbmc_project.EntityManager")
 * @param objectPath  - D-Bus object path (e.g.,
 * "/xyz/openbmc_project/inventory/system")
 * @param interface   - D-Bus interface name (e.g.,
 * "xyz.openbmc_project.Inventory.Item")
 * @param propertyName- Property name (e.g., "Present")
 * @param callback    - Callback function called with (error_code, value)
 */
template <typename T>
void getPropertyAsync(
    std::shared_ptr<sdbusplus::asio::connection> bus,
    const std::string& serviceName, const std::string& objectPath,
    const std::string& interface, const std::string& propertyName,
    std::function<void(boost::system::error_code, T)> callback)
{
    bus->async_method_call(
        [serviceName, propertyName, callback](boost::system::error_code ec,
                                              std::variant<T> value) {
        if (ec)
        {
            lg2::error(
                "Async get property failed: {SERVICE}.{PROPERTY} - {ERROR}",
                "SERVICE", serviceName, "PROPERTY", propertyName, "ERROR",
                ec.message());
            callback(ec, T{});
            return;
        }

        try
        {
            T result = std::get<T>(value);
            callback(boost::system::error_code{}, result);
        }
        catch (const std::bad_variant_access& e)
        {
            lg2::error("Type mismatch for property: {PROPERTY}", "PROPERTY",
                       propertyName);
            callback(boost::system::errc::make_error_code(
                         boost::system::errc::invalid_argument),
                     T{});
        }
    },
        serviceName, objectPath, "org.freedesktop.DBus.Properties", "Get",
        interface, propertyName);
}

/**
 * @brief Asynchronously get D-Bus object subtree
 *
 * @param bus         - Shared pointer to async D-Bus connection
 * @param path        - Root path to search from (e.g.,
 * "/xyz/openbmc_project/inventory")
 * @param depth       - Search depth (0 = unlimited, 1 = only immediate
 * children)
 * @param interfaces  - List of interfaces to match (empty = all)
 * @param callback    - Callback function called with (error_code,
 * GetSubTreeResponse)
 */
inline void getSubTreeAsync(
    std::shared_ptr<sdbusplus::asio::connection> bus, const std::string& path,
    int depth, const std::vector<std::string>& interfaces,
    std::function<void(boost::system::error_code, GetSubTreeResponse)> callback)
{
    bus->async_method_call(
        [path, callback](boost::system::error_code ec,
                         GetSubTreeResponse response) {
        if (ec)
        {
            lg2::error("Async GetSubTree failed for path: {PATH} - {ERROR}",
                       "PATH", path, "ERROR", ec.message());
            callback(ec, GetSubTreeResponse{});
            return;
        }

        lg2::debug(
            "GetSubTree succeeded for path: {PATH}, found {COUNT} objects",
            "PATH", path, "COUNT", response.size());
        callback(boost::system::error_code{}, response);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", path, depth,
        interfaces);
}

/**
 * @brief Asynchronously get all managed objects from a D-Bus service
 *
 * This calls org.freedesktop.DBus.ObjectManager.GetManagedObjects
 * and returns all objects with their interfaces and properties.
 *
 * @param bus         - Shared pointer to async D-Bus connection
 * @param service     - D-Bus service name (e.g., "au.com.codeconstruct.MCTP")
 * @param objectPath  - Object path that implements ObjectManager (e.g.,
 * "/au/com/codeconstruct/mctp1")
 * @param callback    - Callback function called with (error_code,
 * ManagedObjectsResponse)
 */
inline void getManagedObjectsAsync(
    std::shared_ptr<sdbusplus::asio::connection> bus,
    const std::string& service, const std::string& objectPath,
    std::function<void(boost::system::error_code, ManagedObjectsResponse)>
        callback)
{
    bus->async_method_call(
        [service, objectPath, callback](boost::system::error_code ec,
                                        ManagedObjectsResponse response) {
        if (ec)
        {
            lg2::error(
                "Async GetManagedObjects failed for {SERVICE} at {PATH} - {ERROR}",
                "SERVICE", service, "PATH", objectPath, "ERROR", ec.message());
            callback(ec, ManagedObjectsResponse{});
            return;
        }

        lg2::debug(
            "GetManagedObjects succeeded for {SERVICE}, found {COUNT} objects",
            "SERVICE", service, "COUNT", response.size());
        callback(boost::system::error_code{}, response);
    },
        service, objectPath, "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects");
}

/**
 * @brief Asynchronously get all properties from a D-Bus interface
 *
 * Calls org.freedesktop.DBus.Properties.GetAll to retrieve all properties
 * from a specific interface on a D-Bus object.
 *
 * @param bus         - Shared pointer to async D-Bus connection
 * @param serviceName - D-Bus service name (e.g.,
 * "xyz.openbmc_project.EntityManager")
 * @param objectPath  - D-Bus object path (e.g.,
 * "/xyz/openbmc_project/inventory/system/cpu0")
 * @param interface   - D-Bus interface name (e.g.,
 * "xyz.openbmc_project.Inventory.Decorator.LocationContext")
 * @param callback    - Callback function called with (error_code,
 * PropertiesMap)
 */
inline void getAllPropertiesAsync(
    std::shared_ptr<sdbusplus::asio::connection> bus,
    const std::string& serviceName, const std::string& objectPath,
    const std::string& interface,
    std::function<void(boost::system::error_code,
                       std::map<std::string, PropertyValue>)>
        callback)
{
    bus->async_method_call(
        [serviceName, objectPath, interface,
         callback](boost::system::error_code ec,
                   std::map<std::string, PropertyValue> properties) {
        if (ec)
        {
            lg2::error(
                "Async GetAll failed for {SERVICE}.{IFACE} at {PATH} - {ERROR}",
                "SERVICE", serviceName, "IFACE", interface, "PATH", objectPath,
                "ERROR", ec.message());
            callback(ec, std::map<std::string, PropertyValue>{});
            return;
        }

        lg2::debug(
            "GetAll succeeded for {IFACE} at {PATH}, found {COUNT} properties",
            "IFACE", interface, "PATH", objectPath, "COUNT", properties.size());
        callback(boost::system::error_code{}, properties);
    },
        serviceName, objectPath, "org.freedesktop.DBus.Properties", "GetAll",
        interface);
}

/**
 * @brief Asynchronously set a D-Bus property using NVIDIA Async.Set interface
 *
 * Calls com.nvidia.Async.Set.Set method which is an asynchronous setter
 * used by NVIDIA services like NSM. Returns an object path that can be
 * monitored for completion status.
 *
 * @param bus         - Shared pointer to async D-Bus connection
 * @param serviceName - D-Bus service name (e.g., "xyz.openbmc_project.NSM")
 * @param objectPath  - D-Bus object path (e.g.,
 * "/xyz/openbmc_project/inventory/system/accelerator/GPU_0/Processor_Base_Power_Limit")
 * @param interface   - D-Bus interface name (e.g.,
 * "xyz.openbmc_project.Control.Power.Cap")
 * @param propertyName- Property name (e.g., "PowerCap")
 * @param value       - New value to set
 * @param callback    - Callback function called with (error_code,
 * job_object_path) when call is made
 */
template <typename T>
void setPropertyAsyncNvidia(
    std::shared_ptr<sdbusplus::asio::connection> bus,
    const std::string& serviceName, const std::string& objectPath,
    const std::string& interface, const std::string& propertyName,
    const T& value,
    std::function<void(boost::system::error_code, std::string)> callback)
{
    bus->async_method_call(
        [serviceName, propertyName, callback](boost::system::error_code ec,
                                              sdbusplus::object_path jobPath) {
        if (ec)
        {
            lg2::error(
                "Async NVIDIA set property failed: {SERVICE}.{PROPERTY} - {ERROR}",
                "SERVICE", serviceName, "PROPERTY", propertyName, "ERROR",
                ec.message());
            callback(ec, "");
            return;
        }

        lg2::debug(
            "NVIDIA async set property succeeded: {SERVICE}.{PROPERTY}, job: {JOB}",
            "SERVICE", serviceName, "PROPERTY", propertyName, "JOB",
            jobPath.str);
        callback(boost::system::error_code{}, jobPath.str);
    },
        serviceName, objectPath, "com.nvidia.Async.Set", "Set", interface,
        propertyName, std::variant<T>(value));
}

/**
 * @brief Asynchronously log a Redfish event
 *
 * @param bus         - Shared pointer to async D-Bus connection
 * @param messageId   - Redfish message ID (e.g.,
 * "ResourceEvent.1.0.ResourceErrorsDetected")
 * @param severity    - Log severity level
 * @param eventData   - Additional event data (key-value pairs)
 * @param callback    - Optional callback function called with (error_code)
 */
inline void logRedfishEventAsync(
    std::shared_ptr<sdbusplus::asio::connection> bus,
    const std::string messageId, const std::string severity,
    const std::map<std::string, std::string> eventData,
    std::function<void(boost::system::error_code)> callback = nullptr)
{
    // Construct additional data with REDFISH_MESSAGE_ID
    std::map<std::string, std::string> additionalData = eventData;
    additionalData["REDFISH_MESSAGE_ID"] = messageId;

    bus->async_method_call(
        [messageId, callback](boost::system::error_code ec) {
        if (ec)
        {
            lg2::error("Failed to log Redfish event {MESSAGE_ID}: {ERROR}",
                       "MESSAGE_ID", messageId, "ERROR", ec.message());
        }
        else
        {
            lg2::info("Logged Redfish event: {MESSAGE_ID}", "MESSAGE_ID",
                      messageId);
        }

        if (callback)
        {
            callback(ec);
        }
    }, "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Create", "Create",
        messageId,     // Message (used as log entry message)
        severity,      // Severity
        additionalData // Additional data (map<string, string>)
    );
}

namespace LogSeverity
{
constexpr auto Emergency = "xyz.openbmc_project.Logging.Entry.Level.Emergency";
constexpr auto Alert = "xyz.openbmc_project.Logging.Entry.Level.Alert";
constexpr auto Critical = "xyz.openbmc_project.Logging.Entry.Level.Critical";
constexpr auto Error = "xyz.openbmc_project.Logging.Entry.Level.Error";
constexpr auto Warning = "xyz.openbmc_project.Logging.Entry.Level.Warning";
constexpr auto Notice = "xyz.openbmc_project.Logging.Entry.Level.Notice";
constexpr auto Informational =
    "xyz.openbmc_project.Logging.Entry.Level.Informational";
constexpr auto Debug = "xyz.openbmc_project.Logging.Entry.Level.Debug";
} // namespace LogSeverity

} // namespace utils
