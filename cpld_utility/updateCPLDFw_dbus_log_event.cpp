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

#include "updateCPLDFw_dbus_log_event.h"

#include "libcpld.h"

void emitLogMessage(const char* message, const char* arg0, const char* arg1,
                    const char* severity, const char* resolution)
{
    static sd_bus* bus = NULL;
    sd_bus_default_system(&bus);
    if (bus == NULL)
    {
        fprintf(stderr, "Bus is null");
        return;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = NULL;
    char args[BUFFER_LENGTH];
    char updateMessage[BUFFER_LENGTH];
    snprintf(args, BUFFER_LENGTH, "%s,%s", arg0, arg1);
    snprintf(updateMessage, BUFFER_LENGTH, "Update.1.0.%s", message);
    if (debug_h)
        printf("Attempting call\n");
    if (resolution)
    {
        if (sd_bus_call_method(
                bus, LOG_SERVICE, LOG_PATH, LOG_CREATE_INTERFACE,
                LOG_CREATE_FUNCTION, &err, &reply, LOG_CREATE_SIGNATURE,
                updateMessage, severity, 4, "REDFISH_MESSAGE_ID", updateMessage,
                "REDFISH_MESSAGE_ARGS", args, "namespace", "FWUpdate",
                "xyz.openbmc_project.Logging.Entry.Resolution", resolution) < 0)
        {
            fprintf(stderr, "Unable to call log creation function");
        }
    }
    else
    {
        if (sd_bus_call_method(
                bus, LOG_SERVICE, LOG_PATH, LOG_CREATE_INTERFACE,
                LOG_CREATE_FUNCTION, &err, &reply, LOG_CREATE_SIGNATURE,
                updateMessage, severity, 3, "REDFISH_MESSAGE_ID", updateMessage,
                "REDFISH_MESSAGE_ARGS", args, "namespace", "FWUpdate") < 0)
        {
            fprintf(stderr, "Unable to call log creation function");
        }
    }
    if (debug_h)
        printf("Call completed\n");
}
