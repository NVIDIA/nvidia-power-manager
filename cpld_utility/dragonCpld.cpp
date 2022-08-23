#include "dragonCpld.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

#define BUSY_BIT (1 << 12)
#define FAIL_BIT (1 << 13)
#define DONE_BIT (1 << 8)
#define CONF_BITS ((1 << 23) | (1 << 24) | (1 << 25))

DragonCpld::DragonCpld(int updateBus, bool arbitrator, char *imageName,
                       const char *config)
    : DragonChassisBase(updateBus, 0, arbitrator, imageName), config(config)

{}

DragonCpld::~DragonCpld() {}

int DragonCpld::fwUpdate() {
  int ret = 0;

  ret = loadConfig();
  if (ret)
    goto end;

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

  cpldRegFd = openBus(cpldRegBus);
  if (cpldRegFd < 0) {
    ret = static_cast<int>(UpdateError::ERROR_OPEN_BUS);
    goto close_fd;
  }

  ret = enableConfigurationInterface();
  if (ret)
    goto close_cpld_fd;

  ret = erase(true);
  if (ret)
    goto close_cpld_fd;

  ret = sendImage();
  if (ret) {
    goto close_cpld_fd;
  }

  ret = activate();
  if (ret)
    goto close_cpld_fd;

close_cpld_fd:
  close(cpldRegFd);

close_fd:
  close(fd);

disable_arb:
  if (arb) {
    enableArbitration(false);
  }
end:
  return ret;
}

int DragonCpld::readStatusRegister(uint32_t *value) {
  const char write[] = "\x3c\x00\x00\x00";

  return transfer(fd, 1, address, (uint8_t *)&write, (uint8_t *)value, 4, 4);
}

int DragonCpld::waitBusy(int wait, int errorCode) {
  uint32_t reg;
  int cnt = 0;

  while (cnt < wait) {
    if (readStatusRegister(&reg))
      return static_cast<int>(UpdateError::ERROR_READ_STATUS_REG);
    if ((reg & BUSY_BIT) == 0)
      break;
    sleep(1);
  }
  if (cnt >= wait) {
    return errorCode;
  }
  return 0;
}

int DragonCpld::enableConfigurationInterface() {
  char cmd[5] = "\x74\x08\x00\x00";
  int ret = 0;

  ret = sendData(fd, address, cmd, 4,
                 static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER_WRITE));
  if (ret) {
    return ret;
  }

  ret = waitBusy(5, static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER));
  if (ret) {
    return ret;
  }

  return 0;
}

int DragonCpld::erase(bool reset) {
  char cmd[5] = "\x0e\x04\x00\x00";
  char cmd_reset[5] = "\x46\x00\x00\x00";
  uint32_t reg;
  int ret;

  ret =
      sendData(fd, address, cmd, 4, static_cast<int>(UpdateError::ERROR_ERASE));
  if (ret)
    return ret;

  ret = waitBusy(5, static_cast<int>(UpdateError::ERROR_ERASE));
  if (ret) {
    return ret;
  }
  ret = readStatusRegister(&reg);
  if (ret)
    return static_cast<int>(UpdateError::ERROR_READ_STATUS_REG);

  if (reg & FAIL_BIT)
    return static_cast<int>(UpdateError::ERROR_ERASE);
  if (reset) {
    ret = sendData(fd, address, cmd_reset, 4,
                   static_cast<int>(UpdateError::ERROR_RESET));
    if (ret)
      return ret;
  }

  return 0;
}

int DragonCpld::sendImage() {
  char cmd[20] = "\x70\x00\x00\x01";
  char cmdDone[5] = "\x5e\x00\x00\x00";
  int pageSize = 16;
  int remaining = imageSize;
  int imageOffset = 0;
  uint32_t reg;
  int ret = 0;

  while (remaining > 0) {
    memcpy(&cmd[4], image + imageOffset, pageSize);
    ret = sendData(fd, address, cmd, pageSize + 4,
                   static_cast<int>(UpdateError::ERROR_SEND_IMAGE));
    if (ret)
      goto end;
    ret = waitBusy(5, static_cast<int>(UpdateError::ERROR_SEND_IMAGE_BUSY));
    if (ret) {
      return ret;
    }
  }
  ret = sendData(fd, address, cmdDone, 4,
                 static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE));
  if (ret)
    goto end;
  ret = waitBusy(5, static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_BUSY));
  if (ret) {
    return ret;
  }
  ret = readStatusRegister(&reg);
  if (ret)
    return static_cast<int>(UpdateError::ERROR_READ_STATUS_REG);

  if (!(reg & DONE_BIT)) {
    cleanUp();
    return static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_CHECK);
  }

end:
  return ret;
}

int DragonCpld::waitRefresh(int result, int fd) {
  int ret = 0;
  char value;
  int cnt = 0;

read_refresh:
  ret = read(fd, &value, 1);
  if (ret != 1) {
    ret = static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH);
    goto end;
  }
  if (value != result) {
    if (cnt++ < 5)
      goto read_refresh;
    ret = static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT);
    goto end;
  }
end:
  return ret;
}

int DragonCpld::activate() {
  int refreshFd =
      open("/sys/devices/platform/i2c-arbitrator/cpld_refresh", O_RDONLY);
  char cmd1[3] = "\x10\x80";
  char cmd2[3] = "\x10\x40";
  char cmd_refresh[4] = "\x79\x00\x00";
  int ret = 0;
  int rawBusFd = 0;
  uint32_t reg;

  if (refreshFd < 0)
    return static_cast<int>(UpdateError::ERROR_CPLD_REFRESH_NOT_FOUND);

  ret = sendData(cpldRegFd, cpldRegAddress, cmd1, 2,
                 static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG1));
  if (ret)
    goto end;
  ret = sendData(cpldRegFd, cpldRegAddress, cmd2, 2,
                 static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG2));
  if (ret)
    goto end;
  sleep(1);

  ret = waitRefresh(1, refreshFd);
  if (ret)
    goto end;

  rawBusFd = openBus(cpldRawBus);
  if (rawBusFd < 0) {
    ret = static_cast<int>(UpdateError::ERROR_OPEN_BUS);
    goto end;
  }
  ret = sendData(
      rawBusFd, address, cmd_refresh, 3,
      static_cast<int>(UpdateError::ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED));
  if (ret)
    goto closeBus;

  ret = waitRefresh(0, refreshFd);
  if (ret)
    goto closeBus;

  ret = readStatusRegister(&reg);
  if (ret) {
    ret = static_cast<int>(UpdateError::ERROR_READ_STATUS_REG);
    goto closeBus;
  }

  if ((reg & CONF_BITS) || (reg & BUSY_BIT) || !(reg & DONE_BIT)) {
    ret = static_cast<int>(UpdateError::ERROR_CPLD_REFRESH_FAILED);
    goto closeBus;
  }

closeBus:
  close(rawBusFd);
end:
  close(refreshFd);
  return ret;
}

int DragonCpld::cleanUp() {
  int ret = 0;
  ret = erase(false);
  if (ret)
    return static_cast<int>(UpdateError::ERROR_CLEANUP_ERASE);
  return activate();
}

int DragonCpld::loadConfig() {
  std::ifstream in(config);
  if (!in.good())
    return static_cast<int>(UpdateError::ERROR_CONFIG_NOT_FOUND);

  auto data = nlohmann::json::parse(in, nullptr, false);
  if (data.is_discarded())
    return static_cast<int>(UpdateError::ERROR_CONFIG_FORMAT);

  for (const auto &cpldInfo : data.at("CPLD")) {
    try {
      std::string busS = cpldInfo.at("Bus");
      int busN = stoi(busS);
      if (busN == bus) {
        address = cpldInfo.at("Address");
        cpldRegBus = cpldInfo.at("CpldRegBus");
        cpldRegAddress = cpldInfo.at("CpldRegAddress");
        cpldRawBus = cpldInfo.at("CpldRawBus");
      }
    } catch (const std::exception &e) {
      return static_cast<int>(UpdateError::ERROR_CONFIG_PARSE_FAILED);
    }
  }

  return 0;
}
