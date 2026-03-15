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

#include "jobMonitor.hpp"

#include "utils.hpp"

namespace nvidia::power::balancer
{

std::shared_ptr<JobMonitor> JobMonitor::create(
    std::shared_ptr<sdbusplus::asio::connection> bus,
    const std::string& serviceName, const std::string& jobPath,
    const std::string& deviceName, const std::string& locationContext,
    const uint32_t setPoint, std::chrono::seconds timeoutSeconds,
    CompletionCallback callback)
{
    auto monitor = std::shared_ptr<JobMonitor>(
        new JobMonitor(bus, serviceName, jobPath, deviceName, locationContext,
                       setPoint, timeoutSeconds, std::move(callback)));
    monitor->start();
    return monitor;
}

JobMonitor::JobMonitor(std::shared_ptr<sdbusplus::asio::connection> bus,
                       const std::string& serviceName,
                       const std::string& jobPath,
                       const std::string& deviceName,
                       const std::string& locationContext,
                       const uint32_t setPoint,
                       std::chrono::seconds timeoutSeconds,
                       CompletionCallback callback = nullptr) :
    bus_(bus), serviceName_(serviceName), jobPath_(jobPath),
    deviceName_(deviceName), locationContext_(locationContext),
    setPoint_(setPoint), timeout_(timeoutSeconds),
    completionCallback_(std::move(callback))
{
    lg2::info(
        "JobMonitor created for device {DEVICE} at {LOCATION}, job: {JOB}",
        "DEVICE", deviceName_, "LOCATION", locationContext_, "JOB", jobPath_);
    completed_ = false;
}

JobMonitor::~JobMonitor()
{
    completionCallback_ = nullptr;
    cancel();
}

void JobMonitor::start()
{
    if (completed_)
    {
        return;
    }

    lg2::info("Starting job monitor for {JOB}", "JOB", jobPath_);

    // Create property changed signal handler
    statusSignal_ = std::make_unique<sdbusplus::bus::match_t>(
        static_cast<sdbusplus::bus::bus&>(*bus_),
        sdbusplus::bus::match::rules::propertiesChanged(
            jobPath_, "com.nvidia.Async.Status"),
        [this](sdbusplus::message::message& msg) { handleStatusChanged(msg); });

    // Create timeout timer
    timer_ = std::make_unique<boost::asio::steady_timer>(bus_->get_io_context(),
                                                         timeout_);

    std::weak_ptr<JobMonitor> weak = weak_from_this();

    timer_->async_wait([weak](const boost::system::error_code& ec) {
        auto self = weak.lock();
        if (!self)
        {
            return;
        }
        self->handleTimeout(ec);
    });

    utils::getPropertyAsync<std::string>(
        bus_, serviceName_, jobPath_, "com.nvidia.Async.Status", "Status",
        [weak](boost::system::error_code ec, std::string status) {
        auto self = weak.lock();
        if (!self)
        {
            return;
        }
        if (ec)
        {
            lg2::error("Failed to get status for job {JOB}: {ERROR}", "JOB",
                       self->jobPath_, "ERROR", ec.message());
            return;
        }
        else
        {
            self->handleStatus(status);
        }
    });
}

void JobMonitor::cancel()
{
    if (timer_)
    {
        timer_->cancel();
        timer_.reset();
    }

    statusSignal_.reset();
}

void JobMonitor::handleStatusChanged(sdbusplus::message::message& msg)
{
    if (completed_)
    {
        return;
    }

    std::string interface;
    utils::PropertyMap properties;
    msg.read(interface, properties);

    lg2::debug("Job status changed for {JOB}", "JOB", jobPath_);

    // Check Status property
    auto statusIt = properties.find("Status");
    if (statusIt != properties.end())
    {
        try
        {
            std::string status = std::get<std::string>(statusIt->second);
            lg2::info("Job {JOB} status: {STATUS}", "JOB", jobPath_, "STATUS",
                      status);

            handleStatus(status);
            return;
        }
        catch (const std::bad_variant_access&)
        {
            lg2::warning("Status property is not a string for job {JOB}", "JOB",
                         jobPath_);
        }
    }
}

void JobMonitor::handleTimeout(const boost::system::error_code& ec)
{
    if (completed_)
    {
        return;
    }

    if (ec == boost::asio::error::operation_aborted)
    {
        // Timer was cancelled - job completed
        lg2::error("Timer cancelled for job {JOB}", "JOB", jobPath_);
        return;
    }

    if (ec)
    {
        lg2::error("Timer error for job {JOB}: {ERROR}", "JOB", jobPath_,
                   "ERROR", ec.message());
        complete(true);
        return;
    }

    std::string status =
        "xyz.openbmc_project.Common.Progress.OperationStatus.Timeout";
    handleStatus(status);
}

void JobMonitor::handleStatus(std::string status)
{
    lg2::info("Job {JOB} status: {STATUS}", "JOB", jobPath_, "STATUS", status);
    if (status == "com.nvidia.Async.Status.AsyncOperationStatus.InProgress")
    {
        return;
    }
    else if (status == "com.nvidia.Async.Status.AsyncOperationStatus.Success")
    {
        lg2::info("Job {JOB} completed successfully", "JOB", jobPath_);

        complete(true);
    }
    else if (status == "com.nvidia.Async.Status.AsyncOperationStatus.Timeout")
    {
        lg2::error("Job {JOB} completed with error status {STATUS}", "JOB",
                   jobPath_, "STATUS", status);
        complete(true);
        utils::logRedfishEventAsync(
            bus_, "Base.1.19.InternalError", utils::LogSeverity::Critical,
            {{"xyz.openbmc_project.Logging.Entry.Resolution",
              "Setting GPUViewCPULimitWatts on \"" + deviceName_ +
                  "\" with Value = \"" + std::to_string(setPoint_) +
                  "\" failed. Verify GPU state. If the problem persists, "
                  "consider resetting the service."}});
    }
    else
    {
        lg2::error("Job {JOB} completed with error status {STATUS}", "JOB",
                   jobPath_, "STATUS", status);

        complete(true);
        utils::logRedfishEventAsync(
            bus_, "Base.1.19.InternalError", utils::LogSeverity::Critical,
            {{"xyz.openbmc_project.Logging.Entry.Resolution",
              "Setting GPUViewCPULimitWatts on \"" + deviceName_ +
                  "\" with Value = \"" + std::to_string(setPoint_) +
                  "\" failed. Verify GPU state. If the problem persists, "
                  "consider resetting the service."}});
    }
}

void JobMonitor::complete(bool success)
{
    if (completed_)
    {
        return;
    }

    completed_ = true;
    cancel();

    if (completionCallback_)
    {
        completionCallback_(success, jobPath_);
    }
}

} // namespace nvidia::power::balancer
