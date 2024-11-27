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

#include <systemd/sd-bus.h>

// Message Register Variables
#define BUFFER_LENGTH 1024

#define BUSCTL_COMMAND "busctl"
#define LOG_SERVICE "xyz.openbmc_project.Logging"
#define LOG_PATH "/xyz/openbmc_project/logging"
#define LOG_CREATE_INTERFACE "xyz.openbmc_project.Logging.Create"
#define LOG_CREATE_FUNCTION "Create"
#define LOG_CREATE_SIGNATURE "ssa{ss}"

constexpr const char* target_deter = "TargetDetermined";
constexpr const char* ver_failed = "VerificationFailed";
constexpr const char* update_succ = "UpdateSuccessful";
constexpr const char* await_act = "AwaitToAcivate";
constexpr const char* perf_power =
    "Perform AC Power Cycle to activate programmed Firmware";
constexpr const char* sev_info =
    "xyz.openbmc_project.Logging.Entry.Level.Informational";
constexpr const char* sev_crit =
    "xyz.openbmc_project.Logging.Entry.Level.Critical";

/* no return, we will call and fail silently if busctl isn't present */
void emitLogMessage(const char* message, const char* arg0, const char* arg1,
                    const char* severity, const char* resolution);
