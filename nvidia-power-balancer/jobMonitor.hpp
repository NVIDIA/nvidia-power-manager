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

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus/match.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace nvidia::power::balancer
{

class JobMonitor : public std::enable_shared_from_this<JobMonitor>
{
  public:
    using CompletionCallback =
        std::function<void(bool success, const std::string& jobPath)>;

    /**
     * @brief Factory method to create and start a JobMonitor
     *
     * @param bus         - D-Bus connection
     * @param serviceName - D-Bus service name
     * @param jobPath     - Object path of the job to monitor
     * @param deviceName  - Device name for logging
     * @param locationContext - Location context for logging
     * @param setPoint    - Power cap setpoint value
     * @param timeoutSeconds - Timeout in seconds (default: 60)
     * @param callback    - Callback when job completes or times out
     * @return shared_ptr to the created JobMonitor
     */
    static std::shared_ptr<JobMonitor>
        create(std::shared_ptr<sdbusplus::asio::connection> bus,
               const std::string& serviceName, const std::string& jobPath,
               const std::string& deviceName,
               const std::string& locationContext, const uint32_t setPoint,
               std::chrono::seconds timeoutSeconds,
               CompletionCallback callback);

    ~JobMonitor();

    // Delete copy and move (enable_shared_from_this doesn't work well with
    // move)
    JobMonitor(const JobMonitor&) = delete;
    JobMonitor& operator=(const JobMonitor&) = delete;
    JobMonitor(JobMonitor&&) = delete;
    JobMonitor& operator=(JobMonitor&&) = delete;

    /**
     * @brief Cancel monitoring
     */
    void cancel();

    /**
     * @brief Get the job path being monitored
     */
    const std::string& getJobPath() const
    {
        return jobPath_;
    }

  private:
    JobMonitor(std::shared_ptr<sdbusplus::asio::connection> bus,
               const std::string& serviceName, const std::string& jobPath,
               const std::string& deviceName,
               const std::string& locationContext, const uint32_t setPoint,
               std::chrono::seconds timeoutSeconds,
               CompletionCallback callback);

    void start();
    std::shared_ptr<sdbusplus::asio::connection> bus_;
    std::string serviceName_;
    std::string jobPath_;
    std::string deviceName_;
    std::string locationContext_;
    uint32_t setPoint_;
    std::chrono::seconds timeout_;
    CompletionCallback completionCallback_;

    std::unique_ptr<sdbusplus::bus::match_t> statusSignal_;
    std::unique_ptr<boost::asio::steady_timer> timer_;
    bool completed_ = false;

    void handleStatusChanged(sdbusplus::message::message& msg);
    void handleStatus(std::string status);
    void handleTimeout(const boost::system::error_code& ec);
    void complete(bool success);
};

} // namespace nvidia::power::balancer
