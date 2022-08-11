#include "dragonCpld.hpp"

#define BUSY_BIT (1 << 12)
#define FAIL_BIT (1 << 13)

DragonCpld::DragonCpld(int updateBus, int updateAddress, bool arbitrator,
                       char *imageName, int cpldRegBus, int cpldRegAddress)
    : DragonChassisBase(updateBus, updateAddress, arbitrator, imageName),
      cpldRegBus(cpldRegBus), cpldRegAddress(cpldRegAddress)

{}

DragonCpld::~DragonCpld() {}

int DragonCpld::fwUpdate() {
  int ret = 0;

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

  ret = erase();
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

  ret = sendData(fd, cmd, 4,
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

int DragonCpld::sendImage() { return 0; }

int DragonCpld::erase() {
  char cmd[5] = "\x0e\x04\x00\x00";
  char cmd_reset[5] = "\x46\x00\x00\x00";
  uint32_t reg;
  int ret;

  ret = sendData(fd, cmd, 4, static_cast<int>(UpdateError::ERROR_ERASE));
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

  ret = sendData(fd, cmd_reset, 4, static_cast<int>(UpdateError::ERROR_RESET));
  if (ret)
    return ret;

  return 0;
}

int DragonCpld::activate() { return 0; }
