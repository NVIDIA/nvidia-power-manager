/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
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
#include "updateCPLDFw_dbus_log_event.h"

int wait_until_not_busy(int fd, unsigned int delay)
{
    unsigned char write_buffer[8];
    unsigned char read_buffer[4];
    int times_out;
    int ret = 0;

    times_out = 0;
    do
    {
        usleep(delay);
        memset(write_buffer, 0x00, sizeof(write_buffer));
        memset(read_buffer, 0x00, sizeof(read_buffer));

        if (times_out >= TRY_TIMES)
            break;

        write_buffer[0] = 0x00;
        write_buffer[1] = 0x20;
        write_buffer[2] = 0x00;
        write_buffer[3] = 0x20;

        ret = std_send_i2c_cmd(fd, FPGA_READ, MID_I2C_ADDR, write_buffer,
                               read_buffer, 4, 4);
        if (ret)
            return -ERROR_IOCTL_I2C_RDWR_FAILURE;
        if (debug_l)
        {
            printf("read_buffer: %02x %02x %02x %02x\n", read_buffer[0],
                   read_buffer[1], read_buffer[2], read_buffer[3]);
        }
    } while (read_buffer[0] & STATUS_BUSY);

    return 0;
}

int flash_remote_mb_fpga_image(int bus, int image_sel, char* image,
                               int* flashing_progress, char* id)
{
    unsigned char bytes[MB_PAGE_SIZE_BYTES] = {0};
    unsigned char* fw_buf = NULL;
    char i2c_device[MAX_NAME_SIZE] = {0};
    unsigned int mb_img0_start_addr;
    unsigned int mb_img1_start_addr;
    unsigned char write_buffer[8];
    unsigned char read_buffer[8];
    unsigned char* ptr;
    int page_index = 0;
    int fw_file = -1;
    struct stat st;
    int fd = -1;
    int ret = 0;
    int pages;
    unsigned int i;
    ssize_t read_ret;

    sprintf(i2c_device, "/dev/i2c-%d", bus);
    if (debug_h)
    {
        printf("[DEBUG]%s, %s\n", __func__, i2c_device);
    }
    if (debug_l)
    {
        printf("[DEBUG]%s, %s\n", __func__, i2c_device);
    }
    fd = open(i2c_device, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        printf("Error opening file: %s\n", strerror(errno));
        ret = -ERROR_OPEN_I2C_DEVICE;
        goto exit;
    }
    emitLogMessage(target_deter, id, versionStr, sev_info, NULL);
    // Read F/W data first!
    fw_file = open(image, O_RDONLY);
    if (fw_file < 0)
    {
        printf("Error opening file: %s\n", strerror(errno));
        ret = -ERROR_OPEN_FIRMWARE;
        emitLogMessage(ver_failed, id, versionStr, sev_crit, NULL);
        goto exit;
    }
    stat(image, &st);
    /* fw_size must be mutiple of MB_PAGE_SIZE_BYTES */
    if ((st.st_size <= 0) || (st.st_size % MB_PAGE_SIZE_BYTES))
    {
        ret = -ERROR_WRONG_FIRMWARE;
        printf("Error size file:%s %d\n", image, (int)st.st_size);
        emitLogMessage(target_deter, id, versionStr, sev_info, NULL);
        goto exit;
    }

    pages = st.st_size /
            MB_PAGE_SIZE_BYTES; // calc total page , file size / page byte 4
    if (debug_h)
    {
        printf("fw_size:%d\n", (int)st.st_size);
        printf("pages:%d\n", pages);
    }
    fw_buf = (unsigned char*)malloc(st.st_size);
    if (fw_buf == NULL)
    {
        ret = -ERROR_MALLOC_FAILURE;
        goto exit;
    }
    read_ret = read(fw_file, fw_buf, st.st_size);
    close(fw_file);
    if (read_ret != st.st_size)
    {
        ret = -ERROR_FILE_READ;
        goto exit;
    }
    printf("\nFirmware data loaded %d pages.......\n", pages);

    // Get MB_FPGA version
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_buffer, 0x00, sizeof(read_buffer));
    write_buffer[0] = MB_VER_ADDR;
    ret = std_send_i2c_cmd(fd, FPGA_READ, MB_REG_ADDR, write_buffer,
                           read_buffer, 1, 2);
    if (ret)
        goto exit;
    if (debug_h)
    {
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
    if (image_sel == 1)
    { // MB FPGA image 1
        write_buffer[7] = 0xfc;
    }
    else if (image_sel == 0)
    { // MB FPGA image 0
        write_buffer[7] = 0xfb;
    }
    else
    {
        ret = -ERROR_IMAGE_SEL;
        goto exit;
    }
    if (debug_l)
    {
        for (i = 0; i < 8; i++)
            printf("buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8, 0);
    if (ret)
        goto exit;

    // 2.ERASE CFM2 first
    if (image_sel == 1)
    {
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;

        // 3. Read the busy bit to check erase done
        ret = wait_until_not_busy(
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;

        // 5. Read the busy bit to check erase done
        ret = wait_until_not_busy(
            fd, DELAY_1000MS); // 2018.10.01 Jason change to 1 s
        if (ret)
            goto exit;
    }
    else if (image_sel == 0)
    {
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;

        // 5. Read the busy bit to check erase done
        ret = wait_until_not_busy(fd, DELAY_1000MS); // 1ms
        if (ret)
            goto exit;
    }
    else
    {
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
    if (image_sel == 1)
    {
        write_buffer[7] = 0xfc;
    }
    else if (image_sel == 0)
    {
        write_buffer[7] = 0xfb;
    }
    else
    {
        ret = -ERROR_IMAGE_SEL;
        goto exit;
    }
    if (debug_l)
    {
        for (i = 0; i < 8; i++)
            printf("buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
                           0); // 0 = read_buffer none use
    if (ret)
        goto exit;

    // 7. start programming
    printf("\nStart programming FPGA........\n");
    ptr = fw_buf;
    while (page_index < pages)
    {
        /* Begin Address is (MB_CFM12_START_ADDRESS + page_index * 128)/4 */
        mb_img1_start_addr =
            (MB_CFM12_START_ADDRESS + (page_index * MB_PAGE_SIZE_BYTES));
        mb_img0_start_addr =
            (MB_CFM0_START_ADDRESS + (page_index * MB_PAGE_SIZE_BYTES));
        memset(write_buffer, 0x00, sizeof(write_buffer));
        memset(bytes, 0x00, sizeof(bytes));
        if (image_sel == 1)
        {
            write_buffer[0] = (mb_img1_start_addr >> 24) & 0xff;
            write_buffer[1] = (mb_img1_start_addr >> 16) & 0xff;
            write_buffer[2] = (mb_img1_start_addr >> 8) & 0xff;
            write_buffer[3] = (mb_img1_start_addr >> 0) & 0xff;
        }
        else if (image_sel == 0)
        {
            write_buffer[0] = (mb_img0_start_addr >> 24) & 0xff;
            write_buffer[1] = (mb_img0_start_addr >> 16) & 0xff;
            write_buffer[2] = (mb_img0_start_addr >> 8) & 0xff;
            write_buffer[3] = (mb_img0_start_addr >> 0) & 0xff;
        }
        else
        {
            ret = -ERROR_IMAGE_SEL;
            goto exit;
        }
        // reverse high byte
        memcpy((unsigned char*)bytes, (unsigned char*)ptr,
               MB_PAGE_SIZE_BYTES); // write_buffer
        if (debug_l)
        {
            for (i = 0; i < 4; i++)
                printf("rpd byte[%d]:%x\n", i, bytes[i]);
        }
        write_buffer[4] = bytes[3]; // 0xff; //high byte to low byte
        write_buffer[5] = bytes[2]; // 0xe2;
        write_buffer[6] = bytes[1]; // 0xff;
        write_buffer[7] = bytes[0]; // 0x0a;
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("write_buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;
        // check busy bit
        ret = wait_until_not_busy(fd, DELAY_1000MS);
        if (ret)
            goto exit;

        ptr += MB_PAGE_SIZE_BYTES; // ptr+4
        page_index++;
        *flashing_progress = page_index * 99 / pages;
        if (debug_l)
        {
            printf("flashing_progress : %d \n", *flashing_progress);
        }
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
    if (debug_l)
    {
        for (i = 0; i < 8; i++)
            printf("write_buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
                           0); // 0 = read_buffer none use
    if (ret)
        goto exit;
    if (ret == 0)
    {
        printf("\nUpdate successfully!!\n");
        emitLogMessage(update_succ, id, versionStr, sev_info, NULL);
        emitLogMessage(await_act, versionStr, id, sev_info, perf_power);
    }

    // Get MB_FPGA version
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_buffer, 0x00, sizeof(read_buffer));
    write_buffer[0] = MB_VER_ADDR;
    ret = std_send_i2c_cmd(fd, FPGA_READ, MB_REG_ADDR, write_buffer,
                           read_buffer, 1, 2);

    if (ret)
        goto exit;
    if (debug_h)
    {
        printf("Final MB_FPGA version: %02x %02x\n", read_buffer[0],
               read_buffer[1]);
    }

    if (ENABLE_MBRECONFIG)
    {
        // reconfig image1
        printf("\nFPGA reconfiguration........\n");
        memset(write_buffer, 0x00, sizeof(write_buffer));
        write_buffer[0] = 0x00;
        write_buffer[1] = 0x20;
        write_buffer[2] = 0x00;
        write_buffer[3] = 0x04;
        if (image_sel == 1)
        {
            write_buffer[4] = 0x03;
        }
        else if (image_sel == 0)
        {
            write_buffer[4] = 0x01;
        }
        else
        {
            ret = -ERROR_IMAGE_SEL;
            goto exit;
        }
        write_buffer[5] = 0x00;
        write_buffer[6] = 0x00;
        write_buffer[7] = 0x00;
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("write_buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("write_buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MB_I2C_ADDR, write_buffer, 0, 8,
                               0); // 0 = read_buffer none use
        if (ret)
            goto exit;
    }

exit:
    if (fw_buf)
    {
        free((unsigned char*)fw_buf);
    }
    if (fw_file != -1)
    {
        close(fw_file);
    }
    if (fd != -1)
    {
        close(fd);
    }
    if (ret)
    {
        printf("!!!!! F A I L (%d) !!!!!!\n", ret);
        printf("!!!!! F A I L (%d) !!!!!!\n", ret);
        printf("!!!!! F A I L (%d) !!!!!!\n", ret);
    }
    *flashing_progress = 99;
    return ret;
}

int flash_remote_mid_fpga_image(int bus, int image_sel, char* image,
                                int* flashing_progress, char* id)
{
    unsigned char bytes[MID_PAGE_SIZE_BYTES] = {0};
    unsigned char* fw_buf = NULL;
    char i2c_device[MAX_NAME_SIZE] = {0};
    unsigned int mid_img0_start_addr;
    unsigned int mid_img1_start_addr;
    unsigned char write_buffer[8];
    unsigned char read_buffer[8];
    unsigned char* ptr;
    int page_index = 0;
    int fw_file = -1;
    struct stat st;
    int fd = -1;
    int ret = 0;
    int pages;
    unsigned int i;
    ssize_t read_ret;

    sprintf(i2c_device, "/dev/i2c-%d", bus);
    if (debug_h)
    {
        printf("[DEBUG]%s, %s\n", __func__, i2c_device);
    }
    if (debug_l)
    {
        printf("[DEBUG]%s, %s\n", __func__, i2c_device);
    }
    fd = open(i2c_device, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        printf("Error opening file: %s\n", strerror(errno));
        ret = -ERROR_OPEN_I2C_DEVICE;
        goto exit;
    }
    emitLogMessage("TargetDetermined", id, versionStr,
                   "xyz.openbmc_project.Logging.Entry.Level.Informational",
                   NULL);
    // Read F/W data first!
    fw_file = open(image, O_RDONLY);
    if (fw_file < 0)
    {
        printf("Error opening file: %s\n", strerror(errno));
        ret = -ERROR_OPEN_FIRMWARE;
        emitLogMessage("VerificationFailed", id, versionStr,
                       "xyz.openbmc_project.Logging.Entry.Level.Critical",
                       NULL);
        goto exit;
    }
    stat(image, &st);
    /* fw_size must be mutiple of MID_PAGE_SIZE_BYTES */
    if ((st.st_size <= 0) || (st.st_size % MID_PAGE_SIZE_BYTES))
    {
        ret = -ERROR_WRONG_FIRMWARE;
        printf("Error size file:%s %d\n", image, (int)st.st_size);
        emitLogMessage("VerificationFailed", id, versionStr,
                       "xyz.openbmc_project.Logging.Entry.Level.Critical",
                       NULL);
        goto exit;
    }

    pages = st.st_size /
            MID_PAGE_SIZE_BYTES; // calc total page , file size / page byte 8
    if (debug_h)
    {
        printf("fw_size:%d\n", (int)st.st_size);
        printf("pages:%d\n", pages);
    }
    fw_buf = (unsigned char*)malloc(st.st_size);
    if (fw_buf == NULL)
    {
        ret = -ERROR_MALLOC_FAILURE;
        goto exit;
    }
    read_ret = read(fw_file, fw_buf, st.st_size);
    close(fw_file);
    if (read_ret != st.st_size)
    {
        ret = -ERROR_FILE_READ;
        goto exit;
    }

    printf("\nFirmware data loaded %d pages.......\n", pages);

    // Get MID_FPGA version
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_buffer, 0x00, sizeof(read_buffer));
    write_buffer[0] = MID_VER_ADDR;
    ret = std_send_i2c_cmd(fd, FPGA_READ, MID_REG_ADDR, write_buffer,
                           read_buffer, 1, 2);
    if (ret)
        goto exit;
    if (debug_h)
    {
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
    if (image_sel == 3)
    { // MID FPGA image 1
        write_buffer[7] = 0xfc;
    }
    else if (image_sel == 2)
    { // MID FPGA image 0, backup
        write_buffer[7] = 0xfb;
    }
    else
    {
        ret = -ERROR_IMAGE_SEL;
        goto exit;
    }
    if (debug_l)
    {
        for (i = 0; i < 8; i++)
            printf("buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8, 0);
    if (ret)
        goto exit;

    // 2.ERASE CFM2 first
    if (image_sel == 3)
    {
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;

        // 3. Read the busy bit to check erase done
        ret = wait_until_not_busy(
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;

        // 5. Read the busy bit to check erase done
        ret = wait_until_not_busy(
            fd, DELAY_1000MS); // 2018.10.01 Jason change to 1 s
        if (ret)
            goto exit;
    }
    else if (image_sel == 2)
    {
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;

        // 5. Read the busy bit to check erase done
        ret = wait_until_not_busy(fd, DELAY_1000MS); // 1ms
        if (ret)
            goto exit;
    }
    else
    {
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
    if (image_sel == 3)
    {
        write_buffer[7] = 0xfc;
    }
    else if (image_sel == 2)
    {
        write_buffer[7] = 0xfb;
    }
    else
    {
        ret = -ERROR_IMAGE_SEL;
        goto exit;
    }
    if (debug_l)
    {
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
    while (page_index < pages)
    {
        /* Begin Address is (MID_CFM12_START_ADDRESS + page_index * 128)/4 */
        mid_img1_start_addr =
            (MID_CFM12_START_ADDRESS + (page_index * MID_PAGE_SIZE_BYTES));
        mid_img0_start_addr =
            (MID_CFM0_START_ADDRESS + (page_index * MID_PAGE_SIZE_BYTES));
        memset(write_buffer, 0x00, sizeof(write_buffer));
        memset(bytes, 0x00, sizeof(bytes));
        if (image_sel == 3)
        {
            write_buffer[0] = (mid_img1_start_addr >> 24) & 0xff;
            write_buffer[1] = (mid_img1_start_addr >> 16) & 0xff;
            write_buffer[2] = (mid_img1_start_addr >> 8) & 0xff;
            write_buffer[3] = (mid_img1_start_addr >> 0) & 0xff;
        }
        else if (image_sel == 2)
        {
            write_buffer[0] = (mid_img0_start_addr >> 24) & 0xff;
            write_buffer[1] = (mid_img0_start_addr >> 16) & 0xff;
            write_buffer[2] = (mid_img0_start_addr >> 8) & 0xff;
            write_buffer[3] = (mid_img0_start_addr >> 0) & 0xff;
        }
        else
        {
            ret = -ERROR_IMAGE_SEL;
            goto exit;
        }
        // reverse high byte
        memcpy((unsigned char*)bytes, (unsigned char*)ptr,
               MID_PAGE_SIZE_BYTES); // write_buffer
        if (debug_l)
        {
            for (i = 0; i < 4; i++)
                printf("rpd byte[%d]:%x\n", i, bytes[i]);
        }
        write_buffer[4] = bytes[3]; // 0xff; //high byte to low byte
        write_buffer[5] = bytes[2]; // 0xe2;
        write_buffer[6] = bytes[1]; // 0xff;
        write_buffer[7] = bytes[0]; // 0x0a;
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("write_buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                               0);
        if (ret)
            goto exit;
        // check busy bit
        ret = wait_until_not_busy(fd, DELAY_1000MS);
        if (ret)
            goto exit;

        ptr += MID_PAGE_SIZE_BYTES; // ptr+4
        page_index++;
        *flashing_progress = page_index * 99 / pages;
        if (debug_l)
        {
            printf("flashing_progress : %d \n", *flashing_progress);
        }
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
    if (debug_l)
    {
        for (i = 0; i < 8; i++)
            printf("write_buf[%d]:%x\n", i, write_buffer[i]);
    }
    ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                           0); // 0 = read_buffer none use
    if (ret)
        goto exit;
    if (ret == 0)
    {
        printf("\nUpdate successfully!!\n");
        emitLogMessage("UpdateSuccessful", id, versionStr,
                       "xyz.openbmc_project.Logging.Entry.Level.Informational",
                       NULL);
        emitLogMessage(
            "AwaitToActivate", versionStr, id,
            "xyz.openbmc_project.Logging.Entry.Level.Informational",
            "Perform AC Power Cycle to activate programmed Firmware");
    }

    // Get MID_FPGA version
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_buffer, 0x00, sizeof(read_buffer));
    write_buffer[0] = MID_VER_ADDR;
    ret = std_send_i2c_cmd(fd, FPGA_READ, MID_REG_ADDR, write_buffer,
                           read_buffer, 1, 2);
    if (ret)
        goto exit;
    if (debug_h)
    {
        printf("Final MID_FPGA version: %02x %02x\n", read_buffer[0],
               read_buffer[1]);
    }

    if (ENABLE_MIDRECONFIG)
    {
        // reconfig image1
        printf("\nFPGA reconfiguration........\n");
        memset(write_buffer, 0x00, sizeof(write_buffer));
        write_buffer[0] = 0x00;
        write_buffer[1] = 0x20;
        write_buffer[2] = 0x00;
        write_buffer[3] = 0x04;
        if (image_sel == 3)
        {
            write_buffer[4] = 0x03;
        }
        else if (image_sel == 2)
        {
            write_buffer[4] = 0x01;
        }
        else
        {
            ret = -ERROR_IMAGE_SEL;
            goto exit;
        }
        write_buffer[5] = 0x00;
        write_buffer[6] = 0x00;
        write_buffer[7] = 0x00;
        if (debug_l)
        {
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
        if (debug_l)
        {
            for (i = 0; i < 8; i++)
                printf("write_buf[%d]:%x\n", i, write_buffer[i]);
        }
        ret = std_send_i2c_cmd(fd, FPGA_WRITE, MID_I2C_ADDR, write_buffer, 0, 8,
                               0); // 0 = read_buffer none use
        if (ret)
            goto exit;
    }

exit:
    if (fw_buf)
    {
        free((unsigned char*)fw_buf);
    }
    if (fw_file != -1)
    {
        close(fw_file);
    }
    if (fd != -1)
    {
        close(fd);
    }
    if (ret)
    {
        printf("!!!!! F A I L (%d) !!!!!!\n", ret);
        printf("!!!!! F A I L (%d) !!!!!!\n", ret);
        printf("!!!!! F A I L (%d) !!!!!!\n", ret);
    }
    *flashing_progress = 99;
    return ret;
}
