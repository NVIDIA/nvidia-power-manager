/*
// Copyright (c) 2023 Nvidia Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "dragonDeltaPsu.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

void printUsage() {
  std::cout << "The code must be called as psufwupdate fwupgrade BUS "
               "ADDRESS(8-bit) "
               "FILE_TO_FLASH"
            << std::endl;
}

int main(int argc, char **argv) {
  int bus = 0;
  int address = 0;
  bool arbitrator = true;
  const char fwIdentifier[] = "\x32\x32\x30\x30\x32\x30\x36\x35\x44\x43\x45";
  char *imageName = nullptr;
  int ret = 0;

  if (argc < 4) {
    printUsage();
    return -1;
  }

  bus = atoi(argv[2]);
  address = atoi(argv[3]);
  imageName = argv[4];

  if (address <= 0 || bus < 0 || imageName == nullptr) {
    std::cout << "bus, address and image name must be specified" << std::endl;
    printUsage();
    return 0;
  }

  DragonDeltaPsu dp(bus, address, arbitrator, fwIdentifier, imageName);
  ret = dp.fwUpdate();
  if (ret &&
      ret != static_cast<int>(
                 DragonChassisBase::UpdateError::ERROR_WRONG_MANUFACTURER)) {
    dp.printError(ret);
    goto end;
  } else if (ret ==
             static_cast<int>(
                 DragonChassisBase::UpdateError::ERROR_WRONG_MANUFACTURER)) {
    // handle LiteON PSU
  }
end:
  return ret;
}
