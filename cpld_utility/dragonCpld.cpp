#include "dragonCpld.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

#define BUSY_BIT (1 << 12)
#define FAIL_BIT (1 << 13)
#define DONE_BIT (1 << 8)
#define CONF_BITS ((1 << 23) | (1 << 24) | (1 << 25))
#define PAGE_SIZE 16

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

  ret = readDeviceId();
  if (ret)
    goto close_cpld_fd;

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

  return transfer(fd, 1, address, (uint8_t *)write, (uint8_t *)value, 4, 4);
}

int DragonCpld::waitBusy(int wait, int errorCode) {
  int cnt = 0;
  char cmd[] = "\xf0\x00\x00\x00";
  char v;

  while (cnt < wait) {
    transfer(fd, 1, address, (uint8_t *)cmd, (uint8_t *)&v, 4, 1);
    if (!(v & 0x80))
      break;
    sleep(1);
  }
  if (cnt >= wait) {
    return errorCode;
  }
  return 0;
}

int DragonCpld::readDeviceId() {
  uint8_t cmd[] = {0xe0, 0x00, 0x00, 0x00};
  uint8_t id[] = {0x61, 0x2b, 0xb0, 0x43};
  uint8_t dev_id[5];

  transfer(fd, 1, address, (uint8_t *)cmd, dev_id, 4, 4);
  for (int i = 0; i < 4;i++)
    if (id[i] != dev_id[i])
      return static_cast<int>(UpdateError::ERROR_INVALID_DEVICE_ID);
  return 0;
}

int DragonCpld::enableConfigurationInterface() {
  uint8_t cmd[] = {0x74, 0x08, 0x00, 0x00};
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
  uint8_t cmd[] = {0x0E, 0x04, 0x00, 0x00};
  uint8_t cmd_reset[] = {0x46, 0x00, 0x00, 0x00};
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
  uint8_t cmd[(PAGE_SIZE+5)] = {0x70, 0x00, 0x00, 0x01};
  uint8_t cmdDone[] = {0x5E, 0x00, 0x00, 0x00};
  int remaining = imageSize;
  int imageOffset = 0;
  uint32_t reg;
  int ret = 0;

  while (remaining > 0) {
	  int toSend = std::min(remaining, PAGE_SIZE);
	  memset(&cmd[4], 0, PAGE_SIZE);
    memcpy(&cmd[4], image + imageOffset , toSend);
    ret = sendData(fd, address, cmd, PAGE_SIZE + 4,
                   static_cast<int>(UpdateError::ERROR_SEND_IMAGE));
    if (ret)
      goto end;
    ret = waitBusy(5, static_cast<int>(UpdateError::ERROR_SEND_IMAGE_BUSY));
    if (ret) {
      return ret;
    }
	  remaining -= toSend;
  	imageOffset += toSend;
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

  if ((reg & BUSY_BIT) || (reg & FAIL_BIT)) {
    cleanUp();
    return static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_CHECK);
  }
end:
  return ret;
}

int DragonCpld::waitRefresh(int result) {
  int ret = 0;
  int value;
  int cnt = 0;
  std::string refreshPath ="/sys/devices/platform/i2carb/cpld_refresh";

read_refresh:
  value = readSysFs(refreshPath);
  if (value < 0) {
    ret = static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH);
    goto end;
  }
  if (value != result) {
	sleep(1);
    if (cnt++ < 15)
      goto read_refresh;
    if (result)
      ret = static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT);
    else
      ret = static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO);
    goto end;
  }
end:
  return ret;
}

int DragonCpld::activate() {
  uint8_t cmd1[] = {0x10, 0x80};
  uint8_t cmd2[] = {0x10, 0x40};
  uint8_t cmd_refresh[] = {0x79, 0x40, 0x0};
  int ret = 0;
  int rawBusFd = 0;

  ret = sendData(cpldRegFd, cpldRegAddress, cmd1, 2,
                 static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG1));
  if (ret)
    goto end;
  ret = sendData(cpldRegFd, cpldRegAddress, cmd2, 2,
                 static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG2));
  if (ret)
    goto end;
  sleep(1);

  ret = waitRefresh(1);
  if (ret)
    goto end;

  rawBusFd = openBus(cpldRawBus);
  if (rawBusFd < 0) {
    ret = static_cast<int>(UpdateError::ERROR_OPEN_BUS);
    goto end;
  }

  ret = transfer(rawBusFd, 0, address, cmd_refresh, nullptr, 3, 0);
  if (ret) {
    ret = static_cast<int>(UpdateError::ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED);
    goto closeBus;
  }

  ret = waitRefresh(0);
  if (ret)
    goto closeBus;

closeBus:
  close(rawBusFd);
end:
  return ret;
}

int DragonCpld::cleanUp() {
  int ret = 0;
  ret = erase(false);
  if (ret)
    return static_cast<int>(UpdateError::ERROR_CLEANUP_ERASE);

	ret = activate();
	return ret;
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
        std::string ad = cpldInfo.at("Address");
		address = stoi(ad);
        std::string cpldRb = cpldInfo.at("CpldRegBus");
		cpldRegBus = stoi(cpldRb);
        std::string cpldRa = cpldInfo.at("CpldRegAddress");
        cpldRegAddress = stoi(cpldRa);
        std::string cpldRaB = cpldInfo.at("CpldRawBus");
        cpldRawBus = stoi(cpldRaB);
      }
    } catch (const std::exception &e) {
      return static_cast<int>(UpdateError::ERROR_CONFIG_PARSE_FAILED);
    }
  }

  return 0;
}
