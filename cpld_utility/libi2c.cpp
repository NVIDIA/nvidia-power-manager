/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */





#include "libi2c.h"

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
