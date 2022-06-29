#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FPGA_WRITE 0
#define FPGA_READ 1

// MIDPLANE
#define MID_I2C_ADDR 0xD4 // MID FPGA 8 bit update addr
#define MID_REG_ADDR 0x78 // MID FPGA 8 bit reg addr
#define MID_PAGE_SIZE_BYTES 4
#define MID_CFM12_START_ADDRESS 0x08000
#define MID_CFM12_END_ADDRESS 0x49fff
#define MID_CFM0_START_ADDRESS 0x4a000
#define MID_CFM0_END_ADDRESS 0x8bfff
#define MID_CFM_SIZE 0x42000 // end - start +1
#define MAX_NAME_SIZE 255
#define MID_VER_ADDR 0x2D
#define ENABLE_MIDRECONFIG 0

int debug_h = 0;
int debug_l = 0;
// ERROR DEFINITION
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
void show_usage(char *exec) {
  printf("\n\nUsage: %s <i2c bus number> <image select> <firmware filename> "
         "<verbose>\n",
         exec);
  printf("\n        i2c bus number: must be digits [0-13]\n");
  printf("\n        EX: %s 11 1 /tmp/xyz\n\n", exec);
  printf("\n        verbosity(debug)        : -v=High level Debug , -vv=Low "
         "Level Debug \n");
}

/*
 * std_send_i2c_cmd()
 *
 * I2C command function
 *
 * fd: file describe
 * isRead: 1 for READ behavior; 0 for WRITE
 * write_data: write data
 * read_data: read data
 * write_count: write count
 * read_count: read count
 *
 * RETURN: 0 if success
 */
int std_send_i2c_cmd(int fd, int isRead, unsigned char slave,
                     unsigned char *write_data, unsigned char *read_data,
                     unsigned int write_count, unsigned int read_count) {
  struct i2c_rdwr_ioctl_data rdwr_msg;
  struct i2c_msg msg[2];
  int ret;

  memset(&rdwr_msg, 0, sizeof(rdwr_msg));
  memset(&msg, 0, sizeof(msg));

  if (isRead) {
    msg[0].addr = (slave >> 1);
    msg[0].flags = 0;
    msg[0].len = write_count;
    msg[0].buf = write_data;

    msg[1].addr = (slave >> 1);
    msg[1].flags = I2C_M_RD;
    msg[1].len = read_count;
    msg[1].buf = read_data;

    rdwr_msg.msgs = msg;
    rdwr_msg.nmsgs = 2;
  } else {
    msg[0].addr = (slave >> 1);
    msg[0].flags = 0;
    msg[0].len = write_count;
    msg[0].buf = write_data;

    rdwr_msg.msgs = msg;
    rdwr_msg.nmsgs = 1;
  }
  if ((ret = ioctl(fd, I2C_RDWR, &rdwr_msg)) < 0) {
    printf("ret:%d  \n", ret);
    return -ERROR_IOCTL_I2C_RDWR_FAILURE;
  }

  return 0;
}

/*
 * mid_wait_until_not_busy()
 *
 * loop check BUSY status, leave when it's 0 and
 * timeout after TRY_TIMES_MID
 *
 * fd: file describe
 * delay: delay between each check
 *
 *
 * RETURN: 0 if success
 */
#define MID_STATUS_BUSY 0x3
#define TRY_TIMES_MID 5
#define DELAY_1000MS 1000
int mid_wait_until_not_busy(int fd, unsigned int delay) {
  unsigned char write_buffer[8];
  unsigned char read_buffer[4];
  int times_out;
  int ret = 0;

  times_out = 0;
  do {
    usleep(delay);
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_buffer, 0x00, sizeof(read_buffer));

    if (times_out >= TRY_TIMES_MID)
      break;

    write_buffer[0] = 0x00;
    write_buffer[1] = 0x20;
    write_buffer[2] = 0x00;
    write_buffer[3] = 0x20;

    ret = std_send_i2c_cmd(fd, FPGA_READ, MID_I2C_ADDR, write_buffer,
                           read_buffer, 4, 4);
    if (ret)
      return -ERROR_IOCTL_I2C_RDWR_FAILURE;
    if (debug_l) {
      printf("read_buffer: %02x %02x %02x %02x\n", read_buffer[0],
             read_buffer[1], read_buffer[2], read_buffer[3]);
    }
  } while (read_buffer[0] & MID_STATUS_BUSY);

  return 0;
}

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
int checkDigit(char *str) {
  unsigned int i;
  if (atoi(str) > 13)
    return 1;

  for (i = 0; i < strlen(str); i++) {
    if (isdigit(str[i]) == 0)
      return 1;
  }
  return 0;
}

// MID FPGA test

int flash_remote_mid_fpga_image(int bus, int image_sel, char *image,
                                int *flashing_progress) {
  unsigned char bytes[MID_PAGE_SIZE_BYTES] = {0};
  unsigned char *fw_buf = NULL;
  char i2c_device[MAX_NAME_SIZE] = {0};
  unsigned int mid_img0_start_addr;
  unsigned int mid_img1_start_addr;
  unsigned char write_buffer[8];
  unsigned char read_buffer[8];
  unsigned char *ptr;
  int page_index = 0;
  int fw_file = -1;
  struct stat st;
  int fd = -1;
  int ret = 0;
  int pages;
  unsigned int i;

  sprintf(i2c_device, "/dev/i2c-%d", bus);
  if (debug_h) {
    printf("[DEBUG]%s, %s\n", __func__, i2c_device);
  }
  fd = open(i2c_device, O_RDWR | O_NONBLOCK);
  if (fd < 0) {
    printf("Error opening file: %s\n", strerror(errno));
    ret = -ERROR_OPEN_I2C_DEVICE;
    goto exit;
  }
  // Read F/W data first!
  fw_file = open(image, O_RDONLY);
  if (fw_file < 0) {
    printf("Error opening file: %s\n", strerror(errno));
    ret = -ERROR_OPEN_FIRMWARE;
    goto exit;
  }
  stat(image, &st);
  /* fw_size must be mutiple of MID_PAGE_SIZE_BYTES */
  if ((st.st_size <= 0) || (st.st_size % MID_PAGE_SIZE_BYTES)) {
    ret = -ERROR_WRONG_FIRMWARE;
    printf("Error size file:%s %d\n", image, (int)st.st_size);
    goto exit;
  }

  pages = st.st_size /
          MID_PAGE_SIZE_BYTES; // calc total page , file size / page byte 8
  if (debug_h) {
    printf("fw_size:%d\n", (int)st.st_size);
    printf("pages:%d\n", pages);
  }
  fw_buf = (unsigned char *)malloc(st.st_size);
  if (fw_buf == NULL) {
    ret = -ERROR_MALLOC_FAILURE;
    goto exit;
  }
  read(fw_file, fw_buf, st.st_size);
  close(fw_file);
  printf("\nFirmware data loaded %d pages.......\n", pages);

  // Get MID_FPGA version
  memset(write_buffer, 0x00, sizeof(write_buffer));
  memset(read_buffer, 0x00, sizeof(read_buffer));
  write_buffer[0] = 0x2D;
  ret = std_send_i2c_cmd(fd, FPGA_READ, MID_REG_ADDR, write_buffer, read_buffer,
                         1, 2);
  if (ret)
    goto exit;
  if (debug_h) {
    printf("FPGA version: %02x %02x\n", read_buffer[0], read_buffer[1]);
  }

  // 1. unprotect sector CFM1 &CFM2
  printf("\nUnprotect sector CFM1 &CFM2/ CFM0........\n");
  memset(write_buffer, 0x00, sizeof(write_buffer));

  write_buffer[0] = 0x00;
  write_buffer[1] = 0x20;
  write_buffer[2] = 0x00;
  write_buffer[3] = 0x24;
  write_buffer[4] = 0xff;
  write_buffer[5] = 0xff;
  write_buffer[6] = 0xff;
  if (image_sel == 3) { // MID FPGA image 1
    write_buffer[7] = 0xfc;
  } else if (image_sel == 2) { // MID FPGA image 0, backup
    write_buffer[7] = 0xfb;
  } else {
    ret = -ERROR_IMAGE_SEL;
    goto exit;
  }
  if (debug_l) {
    for (i = 0; i < 8; i++)
      printf("buf[%d]:%x\n", i, write_buffer[i]);
  }
  ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8, 0);
  if (ret)
    goto exit;

  // 2.ERASE CFM2 first
  if (image_sel == 3) {
    printf("\nStart erasing FPGA CFM2........\n");
    memset(write_buffer, 0x00, sizeof(write_buffer));
    write_buffer[0] = 0x00;
    write_buffer[1] = 0x20;
    write_buffer[2] = 0x00;
    write_buffer[3] = 0x24;
    write_buffer[4] = 0xff;
    write_buffer[5] = 0xff;
    write_buffer[6] = 0xaf;
    write_buffer[7] = 0xfc;
    if (debug_l) {
      for (i = 0; i < 8; i++)
        printf("buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8, 0);
    if (ret)
      goto exit;

    // 3. Read the busy bit to check erase done
    ret = mid_wait_until_not_busy(
        fd, DELAY_1000MS); // 2018.10.01 Jason change to 1 s
    if (ret)
      goto exit;

    // 4. ERASE CFM1 second
    printf("\nStart erasing FPGA CFM1........\n");
    memset(write_buffer, 0x00, sizeof(write_buffer));
    write_buffer[0] = 0x00;
    write_buffer[1] = 0x20;
    write_buffer[2] = 0x00;
    write_buffer[3] = 0x24;
    write_buffer[4] = 0xff;
    write_buffer[5] = 0xff;
    write_buffer[6] = 0xbf;
    write_buffer[7] = 0xfc;
    if (debug_l) {
      for (i = 0; i < 8; i++)
        printf("buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8, 0);
    if (ret)
      goto exit;

    // 5. Read the busy bit to check erase done
    ret = mid_wait_until_not_busy(
        fd, DELAY_1000MS); // 2018.10.01 Jason change to 1 s
    if (ret)
      goto exit;
  } else if (image_sel == 2) {
    printf("\nStart erasing FPGA CFM0........\n");
    memset(write_buffer, 0x00, sizeof(write_buffer));
    write_buffer[0] = 0x00;
    write_buffer[1] = 0x20;
    write_buffer[2] = 0x00;
    write_buffer[3] = 0x24;
    write_buffer[4] = 0xff;
    write_buffer[5] = 0xff;
    write_buffer[6] = 0xcf;
    write_buffer[7] = 0xfb;
    if (debug_l) {
      for (i = 0; i < 8; i++)
        printf("buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8, 0);
    if (ret)
      goto exit;

    // 5. Read the busy bit to check erase done
    ret = mid_wait_until_not_busy(fd, DELAY_1000MS); // 1ms
    if (ret)
      goto exit;
  } else {
    ret = -ERROR_IMAGE_SEL;
    goto exit;
  }

  // 6. write erase sector bits to default
  printf("\nErasing FPGA sector bits to default........\n");
  memset(write_buffer, 0x00, sizeof(write_buffer));
  write_buffer[0] = 0x00;
  write_buffer[1] = 0x20;
  write_buffer[2] = 0x00;
  write_buffer[3] = 0x24;
  write_buffer[4] = 0xff;
  write_buffer[5] = 0xff;
  write_buffer[6] = 0xff;
  if (image_sel == 3) {
    write_buffer[7] = 0xfc;
  } else if (image_sel == 2) {
    write_buffer[7] = 0xfb;
  } else {
    ret = -ERROR_IMAGE_SEL;
    goto exit;
  }
  if (debug_l) {
    for (i = 0; i < 8; i++)
      printf("buf[%d]:%x\n", i, write_buffer[i]);
  }
  ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                         0); // 0 = read_buffer none use
  if (ret)
    goto exit;

  // 7. start programming
  printf("\nStart programming FPGA........\n");
  ptr = fw_buf;
  while (page_index < pages) {
    /* Begin Address is (MID_CFM12_START_ADDRESS + page_index * 128)/4 */
    mid_img1_start_addr =
        (MID_CFM12_START_ADDRESS + (page_index * MID_PAGE_SIZE_BYTES));
    mid_img0_start_addr =
        (MID_CFM0_START_ADDRESS + (page_index * MID_PAGE_SIZE_BYTES));
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(bytes, 0x00, sizeof(bytes));
    if (image_sel == 3) {
      write_buffer[0] = (mid_img1_start_addr >> 24) & 0xff;
      write_buffer[1] = (mid_img1_start_addr >> 16) & 0xff;
      write_buffer[2] = (mid_img1_start_addr >> 8) & 0xff;
      write_buffer[3] = (mid_img1_start_addr >> 0) & 0xff;
    } else if (image_sel == 2) {
      write_buffer[0] = (mid_img0_start_addr >> 24) & 0xff;
      write_buffer[1] = (mid_img0_start_addr >> 16) & 0xff;
      write_buffer[2] = (mid_img0_start_addr >> 8) & 0xff;
      write_buffer[3] = (mid_img0_start_addr >> 0) & 0xff;
    } else {
      ret = -ERROR_IMAGE_SEL;
      goto exit;
    }
    // reverse high byte
    memcpy((unsigned char *)bytes, (unsigned char *)ptr,
           MID_PAGE_SIZE_BYTES); // write_buffer
    if (debug_l) {
      for (i = 0; i < 4; i++)
        printf("rpd byte[%d]:%x\n", i, bytes[i]);
    }
    write_buffer[4] = bytes[3]; // 0xff; //high byte to low byte
    write_buffer[5] = bytes[2]; // 0xe2;
    write_buffer[6] = bytes[1]; // 0xff;
    write_buffer[7] = bytes[0]; // 0x0a;
    if (debug_l) {
      for (i = 0; i < 8; i++)
        printf("write_buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8, 0);
    if (ret)
      goto exit;
    // check busy bit
    ret = mid_wait_until_not_busy(fd, DELAY_1000MS);
    if (ret)
      goto exit;

    ptr += MID_PAGE_SIZE_BYTES; // ptr+4
    page_index++;
    *flashing_progress = page_index * 99 / pages;
  }
  // re-protect sector CFM
  printf("\nre-protect sector CFM........\n");
  memset(write_buffer, 0x00, sizeof(write_buffer));
  write_buffer[0] = 0x00;
  write_buffer[1] = 0x20;
  write_buffer[2] = 0x00;
  write_buffer[3] = 0x24;
  write_buffer[4] = 0xff;
  write_buffer[5] = 0xff;
  write_buffer[6] = 0xff;
  write_buffer[7] = 0xff;
  if (debug_l) {
    for (i = 0; i < 8; i++)
      printf("write_buf[%d]:%x\n", i, write_buffer[i]);
  }
  ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                         0); // 0 = read_buffer none use
  if (ret)
    goto exit;
  if (ret == 0) {
    printf("\nUpdate successfully!!\n");
  }

  // Get MID_FPGA version
  //
  memset(write_buffer, 0x00, sizeof(write_buffer));
  memset(read_buffer, 0x00, sizeof(read_buffer));
  write_buffer[0] = 0x2D;
  ret = std_send_i2c_cmd(fd, FPGA_READ, MID_REG_ADDR, write_buffer, read_buffer,
                         1, 2);
  if (ret)
    goto exit;
  if (debug_h) {
    printf("Final FPGA version: %02x %02x\n", read_buffer[0], read_buffer[1]);
  }

#ifdef ENABLE_MIDRECONFIG
  // reconfig image1
  printf("\nFPGA reconfiguration........\n");
  memset(write_buffer, 0x00, sizeof(write_buffer));
  write_buffer[0] = 0x00;
  write_buffer[1] = 0x20;
  write_buffer[2] = 0x00;
  write_buffer[3] = 0x04;
  if (image_sel == 3) {
    write_buffer[4] = 0x03;
  } else if (image_sel == 2) {
    write_buffer[4] = 0x01;
  } else {
    ret = -ERROR_IMAGE_SEL;
    goto exit;
  }
  write_buffer[5] = 0x00;
  write_buffer[6] = 0x00;
  write_buffer[7] = 0x00;
  if (debug_l) {
    for (i = 0; i < 8; i++)
      printf("write_buf[%d]:%x\n", i, write_buffer[i]);
  }
  ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                         0); // 0 = read_buffer none use
  if (ret)
    goto exit;

  memset(write_buffer, 0x00, sizeof(write_buffer));
  write_buffer[0] = 0x00;
  write_buffer[1] = 0x20;
  write_buffer[2] = 0x00;
  write_buffer[3] = 0x00;
  write_buffer[4] = 0x01;
  write_buffer[5] = 0x00;
  write_buffer[6] = 0x00;
  write_buffer[7] = 0x00;
  if (debug_l) {
    for (i = 0; i < 8; i++)
      printf("write_buf[%d]:%x\n", i, write_buffer[i]);
  }
  ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                         0); // 0 = read_buffer none use
  if (ret)
    goto exit;
#endif

exit:
  if (fw_buf) {
    free((unsigned char *)fw_buf);
  }
  if (fw_file != -1) {
    close(fw_file);
  }
  if (fd != -1) {
    close(fd);
  }
  if (ret) {
    printf("!!!!! F A I L (%d) !!!!!!\n", ret);
    printf("!!!!! F A I L (%d) !!!!!!\n", ret);
    printf("!!!!! F A I L (%d) !!!!!!\n", ret);
  }
  *flashing_progress = 99;
  return ret;
}

int main(int argc, char *argv[]) {

  int ret = 0;
  if (argc < 4) {
    ret = -ERROR_INPUT_ARGUMENTS;
    show_usage(argv[0]);
    return ret;
  }
  int bus = atoi(argv[1]);
  int image_sel = atoi(argv[2]);
  char *image = argv[3];
  int flashing_progress;
  if (checkDigit(argv[1])) {
    ret = -ERROR_INPUT_I2C_ARGUMENT;
    show_usage(argv[0]);
    return ret;
  }
  if (argc > 4) {
    if (strcmp(argv[4], "-v") == 0)
      debug_h = 1;
    else if (strcmp(argv[4], "-vv") == 0) {
      debug_l = 1;
      debug_h = 1;
    }
  }
  ret = flash_remote_mid_fpga_image(bus, image_sel, image, &flashing_progress);

  return ret;
}
