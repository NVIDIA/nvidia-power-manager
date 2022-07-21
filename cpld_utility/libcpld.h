#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define FPGA_WRITE 0
#define FPGA_READ 1
#define MAX_NAME_SIZE 255
#define STATUS_BUSY 0x3
#define TRY_TIMES 5
#define DELAY_1000MS 1000
#define DEFAULT_VERSION "Unknown"

extern int debug_h;
extern int debug_l;
extern char *versionStr;

// MIDPLANE
#define MID_I2C_ADDR 0xD4 // MID FPGA 8 bit update addr
#define MID_REG_ADDR 0x78 // MID FPGA 8 bit reg addr
#define MID_PAGE_SIZE_BYTES 4
#define MID_CFM12_START_ADDRESS 0x08000
#define MID_CFM12_END_ADDRESS 0x49fff
#define MID_CFM0_START_ADDRESS 0x4a000
#define MID_CFM0_END_ADDRESS 0x8bfff
#define MID_CFM_SIZE 0x42000 // end - start +1
#define MID_VER_ADDR 0x2D
#define ENABLE_MIDRECONFIG 0

// MB,
#define MB_I2C_ADDR 0xD4 // MB FPGA 8 bit update addr
#define MB_REG_ADDR 0x78 // MB FPGA 8 bit reg addr
#define MB_PAGE_SIZE_BYTES 4
#define MB_CFM12_START_ADDRESS 0x04000
#define MB_CFM12_END_ADDRESS 0x26fff
#define MB_CFM0_START_ADDRESS 0x27000
#define MB_CFM0_END_ADDRESS 0x49fff
#define MB_CFM_SIZE 0x23000 // end - start +1
#define MB_VER_ADDR 0x00
#define ENABLE_MBRECONFIG 0

enum {
  ERROR_UNKNOW = 100,
  ERROR_INPUT_ARGUMENTS,
  ERROR_INPUT_I2C_ARGUMENT,
  ERROR_OPEN_FIRMWARE,
  ERROR_WRONG_FIRMWARE,
  ERROR_MALLOC_FAILURE = 105,
  ERROR_OPEN_I2C_DEVICE,
  ERROR_IOCTL_I2C_RDWR_FAILURE,
  ERROR_PROG_BUF_CHECKSUM_ERROR,
  ERROR_PROG_READ_CHECKSUM_ERROR,
  ERROR_PROG_OVER_THREE_TIMES = 110,
  ERROR_CHECKERR_OVER_THREE_TIMES,
  ERROR_IMAGE_SEL,
  ERROR_SET_READMODE,
  ERROR_ERASE,
  ERROR_WRONG_CPLD_DEVICE_SELECTION = 115,
};

/*
 * show_usage()
 *
 * instruction of command
 *
 * exec: program name
 *
 *
 * No RETURN.
 */
void show_usage(char *exec);

/*
 * wait_until_not_busy()
 *
 * loop check BUSY status, leave when it's 0 and
 * timeout after TRY_TIMES
 *
 * fd: file describe
 * delay: delay between each check
 *
 *
 * RETURN: 0 if success
 */

int wait_until_not_busy(int fd, unsigned int delay);

/*
 * checkDigit()
 *
 * confirm if input i2c device is number
 * between 0 ~ 13
 *
 * str: input string
 *
 *
 * RETURN: 0 if success
 */
int checkDigit(char *str);

/*
 * flash_remote_mid_fpga_image()
 *
 * MID FPGA test
 *
 * bus : busnumber
 * image_sel : image selection
 * image : firmare image
 * flashing_progress : flashing_progress
 *
 */
int flash_remote_mid_fpga_image(int bus, int image_sel, char *image,
                                int *flashing_progress, char *id);

/*
 * flash_remote_md_fpga_image()
 *
 * MD FPGA test
 *
 * bus : busnumber
 * image_sel : image selection
 * image : firmare image
 * flashing_progress : flashing_progress
 *
 */
int flash_remote_mb_fpga_image(int bus, int image_sel, char *image,
                               int *flashing_progress, char *id);
