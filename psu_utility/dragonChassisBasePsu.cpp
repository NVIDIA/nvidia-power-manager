#include "dragonDeltaPsu.hpp"

#include <i2c/smbus.h>
#include <sys/stat.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#define CRC_MASK (1 << 6) | (1 << 7)

DragonChassisBasePsu::DragonChassisBasePsu(int bus, int address,
                                           bool arbitrator, char *imageName,
                                           const char *manufacturer,
                                           const char *model)
    : DragonChassisBase(bus, address, arbitrator, imageName),
      manufacturer(manufacturer), model(model) {}

DragonChassisBasePsu::~DragonChassisBasePsu() {}

int DragonChassisBasePsu::checkManufacturer() {
  int ret = 0;
  char read[17];
  char write = 0x99;

  ret = transfer(fd, 1, address, (uint8_t *)&write, (uint8_t *)read, 1, 14);
  if (ret)
    return static_cast<int>(UpdateError::ERROR_READ_MANUFACTURER);
  read[14] = '\0';
  std::string manufacturerBase(manufacturer);
  std::string manufacturerIn((const char *)read);
  boost::to_upper(manufacturerBase);
  boost::to_upper(manufacturerIn);
  if (manufacturerIn.find(manufacturerBase) == std::string::npos) {
    ret = static_cast<int>(UpdateError::ERROR_WRONG_MANUFACTURER);
  }

  write = 0x9A;
  ret = transfer(fd, 1, address, (uint8_t *)&write, (uint8_t *)read, 1, 16);
  if (ret)
    return static_cast<int>(UpdateError::ERROR_READ_MODEL);
  read[16] = '\0';
  std::string modelBase(model);
  std::string modelIn((const char *)read);
  boost::to_upper(modelBase);
  boost::to_upper(modelIn);
  if (modelIn.find(modelBase) == std::string::npos) {
    ret = static_cast<int>(UpdateError::ERROR_WRONG_MODEL);
  }

  return ret;
}

int DragonChassisBasePsu::fwUpdate() {
  int ret = 0;
  int crcCnt = 0;

  ret = loadImage();
  if (ret)
    goto end;

  if (arb) {
    ret = enableArbitration(true);
    if (ret)
      goto end;
  }

  fd = openBus(bus);
  if (fd < 0) {
    ret = static_cast<int>(UpdateError::ERROR_OPEN_BUS);
    goto disable_arb;
  }

  // printFwVersion();

  ret = checkManufacturer();
  if (ret)
    goto close_fd;

  ret = unlock();
  if (ret) {
    goto close_fd;
  }

erase:
  ret = erase();
  if (ret) {
    goto close_fd;
  }

  ret = sendImage();
  if (ret) {
    if (ret != static_cast<int>(UpdateError::ERROR_READ_CRC) && crcCnt < 3) {
      goto close_fd;
    }
    crcCnt++;
    goto erase;
  }

  ret = activate();
  if (ret)
    goto close_fd;
close_fd:
  close(fd);

disable_arb:
  if (arb) {
    enableArbitration(false);
  }
end:
  return ret;
}
