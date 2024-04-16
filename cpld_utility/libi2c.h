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
