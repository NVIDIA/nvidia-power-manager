#include "libcpld.h"

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
                     unsigned int write_count, unsigned int read_count);
