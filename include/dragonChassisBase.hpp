#include "i2c.hpp"

#include <stdint.h>
#include <sys/stat.h>

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
  };

protected:
  int bus;
  int address;
  bool arb;
  char *imageName;
  char *image;
  int imageSize;
  int fd;

  int enableArbitration(bool enable) {
    int arbFd = open("/sys/devices/platform/i2c-arbitrator/arb_state", O_RDWR);
    int ret = 0;
    int error = static_cast<int>(UpdateError::ERROR_DISABLE_ARB);
    char value;
    char valueToWrite = 0;

    if (arbFd < 0)
      return static_cast<int>(UpdateError::ERROR_ARB_NOT_FOUND);

    if (enable) {
      error = static_cast<int>(UpdateError::ERROR_ENABLE_ARB);
      valueToWrite = 1;
    }

    ret = read(arbFd, &value, 1);
    if (ret != 1) {
      ret = error;
      goto end;
    }

    if (value != valueToWrite) {
      int cnt = 0;
      do {
        value = valueToWrite;

        ret = write(arbFd, &valueToWrite, 1);
        if (ret != 1) {
          ret = error;
          goto end;
        }

        ret = read(arbFd, &value, 1);
        if (ret != 1) {
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
    close(arbFd);
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

  int sendData(int send_fd, int send_addr, char *data, int length, int error) {
    int ret = calculate_PEC((uint8_t *)data, address, length);

    if (ret) {
      ret = error;
      goto end;
    }

    ret = transfer(send_fd, 0, send_addr, (uint8_t *)data, nullptr, length + 1,
                   0);
    if (ret) {
      ret |= error;
      goto end;
    }

  end:
    return ret;
  }
  virtual int sendImage() = 0;
};
