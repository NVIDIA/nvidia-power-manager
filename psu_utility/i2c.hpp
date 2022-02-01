#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "crc.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
using namespace std;
int verbose = 0;

#define VERBOSE                                                                \
  if (verbose)                                                                 \
  cout << __func__ << ":" << __LINE__ << "-"

enum class Status {
  ERROR_OPEN_I2C_DEVICE = 109,
  ERROR_IOCTL_I2C_RDWR_FAILURE = 110,
  ERROR_UNKNOW = 0xff,
};

/**
 * I2c is a global  handler.
 */
class I2c : public Crc {
public:
  static int openBus(int localbus) {
    char busCharDev[16];
    std::snprintf(busCharDev, sizeof(busCharDev) - 1, "/dev/i2c-%d", localbus);
    int busFd = open(busCharDev, O_RDWR);
    if (busFd < 0) {
      std::fprintf(stderr, "I2C Bus Open(\"%s\"): \"%s\"\n", busCharDev,
                   strerror(-busFd));
      return static_cast<uint8_t>(Status::ERROR_OPEN_I2C_DEVICE);
    }
    return busFd;
  }

  int calculate_PEC(uint8_t *data, uint32_t slave, int size) {
    int count = 0;
    uint8_t crc = 0;
    uint8_t *PecBytes =
        static_cast<uint8_t *>(malloc((size + 1) * sizeof(uint8_t)));
    if (PecBytes == NULL) {
      return -1;
    }

    VERBOSE << "size: " << size << endl;
    PecBytes[0] = static_cast<uint8_t>(slave);
    memcpy(&PecBytes[1], data, size);

    for (count = 0; count < (size + 1); count++) {
      crc = Crc::crc8((crc ^ PecBytes[count]) << 8);
    }
    data[size] = crc;
    VERBOSE << "data[size]: " << std::hex << static_cast<int>(data[size])
            << endl;

    free(PecBytes);

    return 0;
  }

  /**
   *
   * I2C transfer command function.
   *
   * @param[in] busFd - bus file descriptor.
   * @param[in] isRead - 1 for I2C_READ behavior; 0 for I2C_WRITE.
   * @param[in] write_data - the write data  buffer.
   * @param[in,out] read_data - the read data buffer.
   * @param[in,out] write_count - write count
   * @param[in,out] read_count - read count.
   * @return  0 if success.
   */
  int transfer(int busFd, int isRead, uint32_t slave, uint8_t *write_data,
               uint8_t *read_data, int write_count, int read_count) {

    // Parse message header.
    struct i2c_msg msg[2] = {};
    struct i2c_rdwr_ioctl_data msgset = {};
    VERBOSE << "isRead " << isRead << endl;
    VERBOSE << "write_count - " << write_count << endl;
    for (int i = 0; i < write_count && verbose; i++) {
      cout << std::hex << std::hex << static_cast<int>(write_data[i]) << " ";
    }
    cout << endl;

    if (isRead) {
      VERBOSE << "R[0x" << std::hex << slave << "]" << endl;
      msg[0].addr = static_cast<uint16_t>((slave >> 1));
      msg[0].flags = 0;
      msg[0].len = static_cast<uint16_t>(write_count);
      msg[0].buf = write_data;

      msg[1].addr = static_cast<uint16_t>((slave >> 1));
      msg[1].flags = I2C_M_RD;
      msg[1].len = static_cast<uint16_t>(read_count);
      msg[1].buf = read_data;

      msgset.msgs = msg;
      msgset.nmsgs = 2;
    } else {
      VERBOSE << "W[0x" << std::hex << slave << "]" << endl;
      msg[0].addr = static_cast<uint16_t>((slave >> 1));
      msg[0].flags = 0;
      msg[0].len = static_cast<uint16_t>(write_count);
      msg[0].buf = write_data;

      msgset.msgs = msg;
      msgset.nmsgs = 1;
    }

    int ioError = ioctl(busFd, I2C_RDWR, &msgset);

    if (ioError < 0) {
      std::fprintf(stderr, "I2c::transfer I2C_RDWR ioError=%d?\n", ioError);
      return static_cast<int>(
          Status::ERROR_IOCTL_I2C_RDWR_FAILURE); // I2C_RDWR I/O error
    }

    return 0;
  }
};
