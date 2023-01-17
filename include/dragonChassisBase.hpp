#include "i2c.hpp"

#include <stdint.h>
#include <sys/stat.h>
#include <string>
#include<fstream>

#define ARB_STATE_PATH "/sys/devices/platform/i2carb/arb_state"

class DragonChassisBase : public I2c {
public:
  DragonChassisBase(int bus, int address, bool arbitrator, char *imageName)
      : bus(bus), address(address), arb(arbitrator), imageName(imageName),
        image(nullptr){};

  virtual ~DragonChassisBase() {
    if (image)
      free(image);
  }

  void printError(int code) {

    switch (code) {
    case static_cast<int>(UpdateError::ERROR_LOAD_IMAGE):
      std::cerr << "UpdateError::ERROR_LOAD_IMAGE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ENABLE_ARB):
      std::cerr << "UpdateError::ERROR_ENABLE_ARB" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_DISABLE_ARB):
      std::cerr << "UpdateError::ERROR_DISABLE_ARB" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_OPEN_BUS):
      std::cerr << "UpdateError::ERROR_OPEN_BUS" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_UNLOCK):
      std::cerr << "UpdateError::ERROR_UNLOCK" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ERASE):
      std::cerr << "UpdateError::ERROR_LOAD_IMAGE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_SEND_IMAGE):
      std::cerr << "UpdateError::ERROR_SEND_IMAGE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_SEND_CRC):
      std::cerr << "UpdateError::ERROR_SEND_CRC" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_READ_CRC):
      std::cerr << "UpdateError::ERROR_READ_CRC" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ACTIVATE):
      std::cerr << "UpdateError::ERROR_ACTIVATE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_OPEN_IMAGE):
      std::cerr << "UpdateError::ERROR_OPEN_IMAGE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_STAT_IMAGE):
      std::cerr << "UpdateError::ERROR_STAT_IMAGE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_WRONG_MANUFACTURER):
      std::cerr << "UpdateError::ERROR_WRONG_MANUFACTURER" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_READ_MANUFACTURER):
      std::cerr << "UpdateError::ERROR_READ_MANUFACTURER" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_READ_STATUS_REG):
      std::cerr << "UpdateError::ERROR_READ_STATUS_REG" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER):
      std::cerr << "UpdateError::ERROR_ENABLE_CFG_INTER" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ENABLE_CFG_INTER_WRITE):
      std::cerr << "UpdateError::ERROR_ENABLE_CFG_INTER_WRITE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_RESET):
      std::cerr << "UpdateError::ERROR_RESET" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ARB_NOT_FOUND):
      std::cerr << "UpdateError::ERROR_ARB_NOT_FOUND" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_WRONG_MODEL):
      std::cerr << "UpdateError::ERROR_WRONG_MODEL" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_READ_MODEL):
      std::cerr << "UpdateError::ERROR_READ_MODEL" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_BUSY):
      std::cerr << "UpdateError::ERROR_SEND_IMAGE_BUSY" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE):
      std::cerr << "UpdateError::ERROR_SEND_IMAGE_DONE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_BUSY):
      std::cerr << "UpdateError::ERROR_SEND_IMAGE_DONE_BUSY" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_SEND_IMAGE_DONE_CHECK):
      std::cerr << "UpdateError::ERROR_SEND_IMAGE_DONE_CHECK" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG1):
      std::cerr << "UpdateError::ERROR_ACTIVATE_CPLD_REG1" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_ACTIVATE_CPLD_REG2):
      std::cerr << "UpdateError::ERROR_ACTIVATE_CPLD_REG2" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_CPLD_REFRESH_NOT_FOUND):
      std::cerr << "UpdateError::ERROR_CPLD_REFRESH_NOT_FOUND" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_CPLD_REFRESH_FAILED):
      std::cerr << "UpdateError::ERROR_CPLD_REFRESH_FAILED" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED):
      std::cerr << "UpdateError::ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED"
                << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_INTERNAL_CPLD_REFRESH_FAILED):
      std::cerr << "UpdateError::ERROR_INTERNAL_CPLD_REFRESH_FAILED"
                << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH):
      std::cerr << "UpdateError::ERROR_READ_CPLD_REFRESH" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT):
      std::cerr << "UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT" << std::endl;
      break;
	case static_cast<int>(UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO):
      std::cerr << "UpdateError::ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_CLEANUP_ERASE):
      std::cerr << "UpdateError::ERROR_CLEANUP_ERASE" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_CONFIG_NOT_FOUND):
      std::cerr << "UpdateError::ERROR_CONFIG_NOT_FOUND" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_CONFIG_FORMAT):
      std::cerr << "UpdateError::ERROR_CONFIG_FORMAT" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_CONFIG_PARSE_FAILED):
      std::cerr << "UpdateError::ERROR_CONFIG_PARSE_FAILED" << std::endl;
      break;
    case static_cast<int>(UpdateError::ERROR_INVALID_DEVICE_ID):
      std::cerr << "UpdateError::ERROR_INVALID_DEVICE_ID" << std::endl;
      break;
    case 0:
      std::cerr << "Update Was Successful" << std::endl;
      break;
    default:
      std::cerr << "Invalid error code" << std::endl;
      break;
    }
  }

  enum class UpdateError {
    ERROR_LOAD_IMAGE = 1,
    ERROR_ENABLE_ARB = 2,
    ERROR_DISABLE_ARB = 3,
    ERROR_OPEN_BUS = 4,
    ERROR_UNLOCK = 5,
    ERROR_ERASE = 6,
    ERROR_SEND_IMAGE = 7,
    ERROR_SEND_CRC = 8,
    ERROR_READ_CRC = 9,
    ERROR_ACTIVATE = 10,
    ERROR_OPEN_IMAGE = 11,
    ERROR_STAT_IMAGE = 12,
    ERROR_WRONG_MANUFACTURER = 13,
    ERROR_READ_MANUFACTURER = 14,
    ERROR_READ_STATUS_REG = 15,
    ERROR_ENABLE_CFG_INTER = 16,
    ERROR_ENABLE_CFG_INTER_WRITE = 17,
    ERROR_RESET = 18,
    ERROR_ARB_NOT_FOUND = 19,
    ERROR_WRONG_MODEL = 20,
    ERROR_READ_MODEL = 21,
    ERROR_SEND_IMAGE_BUSY = 22,
    ERROR_SEND_IMAGE_DONE = 23,
    ERROR_SEND_IMAGE_DONE_BUSY = 24,
    ERROR_SEND_IMAGE_DONE_CHECK = 25,
    ERROR_ACTIVATE_CPLD_REG1 = 26,
    ERROR_ACTIVATE_CPLD_REG2 = 27,
    ERROR_CPLD_REFRESH_NOT_FOUND = 28,
    ERROR_CPLD_REFRESH_FAILED = 29,
    ERROR_SEND_INTERNAL_CPLD_REFRESH_FAILED = 30,
    ERROR_INTERNAL_CPLD_REFRESH_FAILED = 31,
    ERROR_READ_CPLD_REFRESH = 32,
    ERROR_READ_CPLD_REFRESH_TIMEOUT = 33,
    ERROR_CLEANUP_ERASE = 34,
    ERROR_CONFIG_NOT_FOUND = 35,
    ERROR_CONFIG_FORMAT = 36,
    ERROR_CONFIG_PARSE_FAILED = 37,
    ERROR_INVALID_DEVICE_ID = 38,
    ERROR_READ_CPLD_REFRESH_TIMEOUT_ZERO = 39,
  };

protected:
  int bus;
  int address;
  bool arb;
  char *imageName;
  char *image;
  int imageSize;
  int fd;

  int readSysFs(std::string path) {
    int ret = 0;
    std::string s;
    std::ifstream f(path.c_str());
    if (!f)
      return -1;
    f >> s;
    f.close();
    try {
      ret = std::stoi(s);
    } catch (const std::exception &e) {
      ret = -1;
    }
    return ret;
  }

  int writeSysFs(std::string path, int value)
  {
    std::ofstream f(path.c_str());
    if (!f)
      return -1;
    f << value;
    f.close();
    return 0;
  }

  int enableArbitration(bool enable) {
    int ret = 0;
    int error = static_cast<int>(UpdateError::ERROR_DISABLE_ARB);
    int value;
    int valueToWrite = 0;

    if (enable) {
      error = static_cast<int>(UpdateError::ERROR_ENABLE_ARB);
      valueToWrite = 1;
    }

    value = readSysFs(ARB_STATE_PATH);
    if (value == -1) {
      ret = error;
      goto end;
    }
    if (value != valueToWrite) {
      int cnt = 0;
      do {
        ret = writeSysFs(ARB_STATE_PATH, valueToWrite);
        if (ret == -1) {
          ret = error;
          goto end;
        }
        value = readSysFs(ARB_STATE_PATH);
        if (value == -1) {
          ret = error;
          goto end;
        }
        cnt++;
      } while (value != valueToWrite && cnt < 5);

      if (value != valueToWrite) {
        ret = error;
        goto end;
      }
    }
  end:
    return ret;
  }

  int loadImage() {
    int loadFd = 0;
    struct stat st;
    int ret = 0;

    loadFd = open(imageName, O_RDONLY);
    if (loadFd < 0) {
      ret = static_cast<int>(UpdateError::ERROR_OPEN_IMAGE);
      goto end;
    }

    stat(imageName, &st);
    imageSize = st.st_size;
    if (imageSize <= 0) {
      ret = static_cast<int>(UpdateError::ERROR_STAT_IMAGE);
      goto close;
    }
    image = (char *)malloc(imageSize);
    if (!image) {
      ret = static_cast<int>(UpdateError::ERROR_LOAD_IMAGE);
      goto close;
    }

    if (read(loadFd, image, imageSize) != imageSize) {
      ret = static_cast<int>(UpdateError::ERROR_LOAD_IMAGE);
      goto close;
    }
  close:
    close(loadFd);
  end:
    return ret;
  }

  int sendData(int send_fd, int send_addr, uint8_t *data, int length, int error) {
	uint8_t *dataWithPec =
        static_cast<uint8_t *>(malloc((length + 1) * sizeof(uint8_t)));

	if (!dataWithPec)
		return error;
	memcpy(dataWithPec, data, length);
    int ret = calculate_PEC(dataWithPec, address, length);

    if (ret) {
      ret = error;
      goto end;
    }

    ret = transfer(send_fd, 0, send_addr, dataWithPec, nullptr, length + 1, 0);
    if (ret) {
      ret |= error;
      goto end;
    }

  end:
	free(dataWithPec);
    return ret;
  }

  virtual int sendImage() = 0;
};
