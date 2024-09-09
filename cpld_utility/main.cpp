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

#ifdef DRAGON_CHASSIS
#include "dragonCpld.hpp"
#endif

#include "libcpld.h"
#define MAX_I2C_BUS_NUMBER 42

const char* DEFAULT_VERSION = "Unknown";
int debug_l;
int debug_h;
const char* versionStr;
char* configFile = NULL;

int checkDigit(char* str)
{
    unsigned int i;
    if (atoi(str) > MAX_I2C_BUS_NUMBER)
        return 1;

    for (i = 0; i < strlen(str); i++)
    {
        if (isdigit(str[i]) == 0)
            return 1;
    }
    return 0;
}

void show_usage(char* exec)
{
    printf("\n\nUsage: %s <i2c bus number> <image select> <firmware filename>  "
           "<CPLD Device selection>  "
           "<verbose>\n",
           exec);
    printf("\n        i2c bus number: must be digits [1-2]\n");
    printf("\n        EX: %s 11 1 /tmp/xyz\n\n", exec);
    printf("\n        verbosity(debug)        : -v=High level Debug , -vv=Low "
           "Level Debug \n");
    printf("\n        CPLD Device Selection   :  1 -> Mother Board   "
           "2 -> Middle Plane \n");
}

int main(int argc, char* argv[])
{
    int ret = 0;
    if (argc < 5)
    {
        ret = -ERROR_INPUT_ARGUMENTS;
        show_usage(argv[0]);
        return ret;
    }
    int bus = atoi(argv[1]);
    int image_sel = atoi(argv[2]);
    char* image = argv[3];
    int cpldDeviceSelection = atoi(argv[4]);
    int flashing_progress;
    char cpld0[] = "cpld0";
    char cpld1[] = "cpld1";

    if (checkDigit(argv[1]))
    {
        ret = -ERROR_INPUT_I2C_ARGUMENT;
        show_usage(argv[0]);
        return ret;
    }

    if (argc > 5)
    {
        versionStr = argv[5];
    }
    else
    {
        versionStr = DEFAULT_VERSION;
    }

    if (argc > 6)
    {
        if (strcmp(argv[6], "-v") == 0)
            debug_h = 1;
        else if (strcmp(argv[6], "-vv") == 0)
        {
            debug_l = 1;
            debug_h = 1;
        }
        else
        {
            configFile = argv[6];
        }
    }
    if (cpldDeviceSelection == 1)
    {
        ret = flash_remote_mb_fpga_image(bus, image_sel, image,
                                         &flashing_progress, cpld0);
    }
    else if (cpldDeviceSelection == 2)
    {
#ifdef DRAGON_CHASSIS
        if (argc > 6)
        {
            DragonCpld dp(bus, image, configFile);
            ret = dp.fwUpdate();
            dp.printError(ret);
        }
        else
#endif
        {
            ret = flash_remote_mid_fpga_image(bus, image_sel, image,
                                              &flashing_progress, cpld1);
        }
    }
    else
    {
        ret = -ERROR_WRONG_CPLD_DEVICE_SELECTION;
        show_usage(argv[0]);
        return ret;
    }

    return ret;
}
