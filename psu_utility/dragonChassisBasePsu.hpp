/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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







#include "dragonChassisBase.hpp"

#include <stdint.h>

/*
* DragonChassisBasePsu class extends DragonChassisBase class.
* the idea is that multiple PSU's can be supported since POR still
* calls for potential two different PSU's being present on Dragon Chassis
*/
class DragonChassisBasePsu : public DragonChassisBase {
public:
  /*
  * Constructor:
  * inputs:
  * bus - i2c bus number
  * address - i2c address
  * arbitrator - is there arbitrator present?
  * imageName - path to image file
  * manufacturer - what is expected manufacurer name to be found?
  * model - what is expected model to be found?
  */
  DragonChassisBasePsu(int bus, int address, bool arbitrator, char *imageName,
                       const char *manufacturer, const char *model);
  virtual ~DragonChassisBasePsu();

  /*
  * fwUpdate - implement fwUpdate flow
  */
  int fwUpdate();

protected:
  const char *manufacturer; //manufacturer name
  const char *model;  //model name

  /*
  * checkManufacturer: check manufacturer name and model in the PMBUS standard registers
  * return - 0 if correct 1 if not correct
  */
  int checkManufacturer();

  /*
  * unlock: unlocks access to the PSU
  * return - 0 if correct 1 if not correct
  */
  virtual int unlock() = 0;

  /*
  * erase: erase PSU content
  * return - 0 if correct 1 if not correct
  */
  virtual int erase() = 0;

  /*
  * activate: activate PSU firmware
  * return - 0 if correct 1 if not correct
  */
  virtual int activate() = 0;

  /*
  * printFwVersion: print PSU FW version
  */
  virtual void printFwVersion() = 0;
};
