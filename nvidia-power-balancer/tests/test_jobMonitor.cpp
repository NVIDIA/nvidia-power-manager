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

#include "jobMonitor.hpp"

#include <string>

#include <gtest/gtest.h>

using namespace nvidia::power::balancer;

// Testable subclass that exposes protected members and allows construction
// without D-Bus
class TestableJobMonitor : public JobMonitor
{
  public:
    TestableJobMonitor() :
        JobMonitor(nullptr, "", "/test/job/path", "GPU_0", "location1", 300,
                   std::chrono::seconds(60), nullptr)
    {}

    TestableJobMonitor(JobMonitor::CompletionCallback callback) :
        JobMonitor(nullptr, "", "/test/job/path", "GPU_0", "location1", 300,
                   std::chrono::seconds(60), callback)
    {}

    using JobMonitor::complete;
    using JobMonitor::completed_;
    using JobMonitor::completionCallback_;
    using JobMonitor::deviceName_;
    using JobMonitor::handleStatus;
    using JobMonitor::jobPath_;
    using JobMonitor::locationContext_;
    using JobMonitor::setPoint_;
};

// ============================================================================
// Test JobMonitor - Only tests that don't require D-Bus
// Note: handleStatus for Timeout/Error paths call logRedfishEventAsync which
//       requires D-Bus, so those tests are excluded.
// ============================================================================

// --- handleStatus tests (only InProgress and Success paths) ---

TEST(JobMonitorTest, HandleStatus_InProgress_DoesNotComplete)
{
    bool callbackCalled = false;
    auto callback = [&callbackCalled](bool, const std::string&) {
        callbackCalled = true;
    };

    TestableJobMonitor monitor(callback);
    EXPECT_FALSE(monitor.completed_);

    monitor.handleStatus(
        "com.nvidia.Async.Status.AsyncOperationStatus.InProgress");

    // Should not complete for InProgress status
    EXPECT_FALSE(monitor.completed_);
    EXPECT_FALSE(callbackCalled);
}

TEST(JobMonitorTest, HandleStatus_Success_CompletesWithSuccess)
{
    bool callbackCalled = false;
    bool callbackSuccess = false;
    std::string callbackJobPath;

    auto callback = [&](bool success, const std::string& jobPath) {
        callbackCalled = true;
        callbackSuccess = success;
        callbackJobPath = jobPath;
    };

    TestableJobMonitor monitor(callback);
    EXPECT_FALSE(monitor.completed_);

    monitor.handleStatus(
        "com.nvidia.Async.Status.AsyncOperationStatus.Success");

    EXPECT_TRUE(monitor.completed_);
    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(callbackSuccess);
    EXPECT_EQ(callbackJobPath, "/test/job/path");
}

// --- complete tests ---

TEST(JobMonitorTest, Complete_SetsCompletedFlag)
{
    TestableJobMonitor monitor;
    EXPECT_FALSE(monitor.completed_);

    monitor.complete(true);

    EXPECT_TRUE(monitor.completed_);
}

TEST(JobMonitorTest, Complete_InvokesCallback)
{
    bool callbackCalled = false;
    bool callbackSuccess = false;
    std::string callbackJobPath;

    auto callback = [&](bool success, const std::string& jobPath) {
        callbackCalled = true;
        callbackSuccess = success;
        callbackJobPath = jobPath;
    };

    TestableJobMonitor monitor(callback);

    monitor.complete(true);

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(callbackSuccess);
    EXPECT_EQ(callbackJobPath, "/test/job/path");
}

TEST(JobMonitorTest, Complete_WithFailure_PassesFailureToCallback)
{
    bool callbackSuccess = true;

    auto callback = [&](bool success, const std::string&) {
        callbackSuccess = success;
    };

    TestableJobMonitor monitor(callback);

    monitor.complete(false);

    EXPECT_FALSE(callbackSuccess);
}

TEST(JobMonitorTest, Complete_AlreadyCompleted_DoesNotInvokeCallbackAgain)
{
    int callbackCount = 0;

    auto callback = [&](bool, const std::string&) { callbackCount++; };

    TestableJobMonitor monitor(callback);

    monitor.complete(true);
    EXPECT_EQ(callbackCount, 1);

    // Second call should be a no-op
    monitor.complete(true);
    EXPECT_EQ(callbackCount, 1);
}

TEST(JobMonitorTest, Complete_NullCallback_DoesNotCrash)
{
    TestableJobMonitor monitor(nullptr);

    // Should not crash with null callback
    monitor.complete(true);

    EXPECT_TRUE(monitor.completed_);
}

// --- State initialization tests ---

TEST(JobMonitorTest, InitialState_NotCompleted)
{
    TestableJobMonitor monitor;
    EXPECT_FALSE(monitor.completed_);
}

TEST(JobMonitorTest, InitialState_HasCorrectJobPath)
{
    TestableJobMonitor monitor;
    EXPECT_EQ(monitor.getJobPath(), "/test/job/path");
}

TEST(JobMonitorTest, InitialState_HasCorrectDeviceInfo)
{
    TestableJobMonitor monitor;
    EXPECT_EQ(monitor.deviceName_, "GPU_0");
    EXPECT_EQ(monitor.locationContext_, "location1");
    EXPECT_EQ(monitor.setPoint_, 300u);
}

// --- Multiple status changes tests (only using Success path) ---

TEST(JobMonitorTest, MultipleInProgressThenSuccess)
{
    int callbackCount = 0;

    auto callback = [&](bool, const std::string&) { callbackCount++; };

    TestableJobMonitor monitor(callback);

    // Simulate multiple InProgress status changes
    monitor.handleStatus(
        "com.nvidia.Async.Status.AsyncOperationStatus.InProgress");
    EXPECT_EQ(callbackCount, 0);

    monitor.handleStatus(
        "com.nvidia.Async.Status.AsyncOperationStatus.InProgress");
    EXPECT_EQ(callbackCount, 0);

    // Then success
    monitor.handleStatus(
        "com.nvidia.Async.Status.AsyncOperationStatus.Success");
    EXPECT_EQ(callbackCount, 1);

    // Further calls should be ignored (already completed)
    monitor.handleStatus(
        "com.nvidia.Async.Status.AsyncOperationStatus.Success");
    EXPECT_EQ(callbackCount, 1);
}
