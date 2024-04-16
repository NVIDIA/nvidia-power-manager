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






#include "i2c.hpp"
#include "util.hpp"
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <vector>

#define MAXFILESIZE 0x64

#define MTD_DEV_SIZE 20

#define INT32U uint32_t

char MTD_Device[MTD_DEV_SIZE];

typedef struct {
  INT32U FlashOffset;
  INT32U Size;
  INT32U SectionFlash;
  int BMCInst;
  INT32U PSUPrntNum;
} FlashInfo_T;

typedef struct {
  unsigned short program_mode_time;
  unsigned short write_time;
  unsigned short block_size;
} FirmwareDownloadPara_T;

#define DELTA_BYTE_PER_PACKET (0x10)

#define PSU_MAX_NUM 6
#define I2C_ADDR_0x80 0x80
#define I2C_ADDR_0x82 0x82
#define I2C_ADDR_0x84 0x84
#define PSU_WRITE 0
#define PSU_READ 1

#define MCU_MAX_NUM 3 // Delta*3
#define MCU_ID_PRIM 0x10
#define MCU_ID_SEC 0x20
#define MCU_ID_COM 0x30
#define DELAY_PRIM 55
#define DELAY_SEC 55
#define DELAY_COM 5
#define PRIM_IMA_SIZE 28160 //	256*110
#define SEC_IMA_SIZE 16896  //	256*66
#define COM_IMA_SIZE 56320  //	256*220
#define MAX_BUFFER_SIZE 1024
#define TRANS_ERR 0x20
#define BUSY_ERR 0x10
#define REG_FIRMWARE_UNLOCK 0xF0
#define REG_BOOT_STATUS_MODE 0xF1
#define REG_TRANSMIT_RAM 0xF2
#define REG_TRANSMIT_FLASH 0xF3
#define REG_TRANSMIT_CRC 0xF4
#define DELAY_MS 1000
#define DELAY_500_MS 500000
#define DELAY_1_S 1000000
#define DELAY_5_S 5000000

typedef struct {
  uint32_t PsuNo;
  uint32_t SlaveAddr;
} PSUSlave_T;

typedef struct {
  uint32_t SectionFlash;
  uint32_t FwSize;
  uint8_t McuID;          // MCU_ID;
  uint32_t BytePerPage;   // BYTE_PER_PAGE;
  uint32_t BytePerPacket; // BYTE_PER_PACKET;
  uint32_t BootSTA;       // BOOT_STA;
  uint32_t AppCrcSTA;     // APPCRC_STA
  uint32_t Delay1;
  uint32_t Delay2;
  uint32_t Delay3;

} MCUPart_T;

// ERROR DEFINITION
enum class ErrorStatus {
  ERROR_INPUT_ARGUMENTS = 100,
  ERROR_INPUT_I2C_ARGUMENT,
  ERROR_INPUT_PSU_ARGUMENT,
  ERROR_INPUT_MCU_ARGUMENT,
  ERROR_INPUT_CKS_ARGUMENT,
  ERROR_OPEN_FIRMWARE = 105,
  ERROR_WRONG_FIRMWARE,
  ERROR_WRONG_CRC16_CHKSM,
  ERROR_MALLOC_FAILURE,
  ERROR_OPEN_I2C_DEVICE,
  ERROR_IOCTL_I2C_RDWR_FAILURE = 110,
  ERROR_PROG_BUF_CHECKSUM_ERROR,
  ERROR_PROG_READ_CHECKSUM_ERROR,
  ERROR_PROG_OVER_THREE_TIMES,
  ERROR_CHECKERR_OVER_THREE_TIMES,
  ERROR_ENTER_BOOTLOADER = 115, //
  ERROR_TRANS_BLOCK,
  ERROR_TRANS_PAGE,
  ERROR_UPG_FAIL,
  ERROR_UNKNOW = 0xff,
};

auto statusToString(ErrorStatus s) -> const char * {
  using E = ErrorStatus;
  if (s == E::ERROR_INPUT_ARGUMENTS)
    return "Error: input Arguments";
  if (s == E::ERROR_INPUT_I2C_ARGUMENT)
    return "Error: input I2C Argument";
  if (s == E::ERROR_INPUT_PSU_ARGUMENT)
    return "Error: input PSU Argument";
  if (s == E::ERROR_INPUT_MCU_ARGUMENT)
    return "Error: input MCU Argument";
  if (s == E::ERROR_INPUT_CKS_ARGUMENT)
    return "Error: input CKS Argument";
  if (s == E::ERROR_OPEN_FIRMWARE)
    return "Error: Opening Firmware image";
  if (s == E::ERROR_WRONG_FIRMWARE)
    return "Error: Invalid Firmware image";
  if (s == E::ERROR_WRONG_CRC16_CHKSM)
    return "Error: Wrong CRC16 checksum";
  if (s == E::ERROR_MALLOC_FAILURE)
    return "Error: Malloc failure";
  if (s == E::ERROR_OPEN_I2C_DEVICE)
    return "Error: Opening I2C device failure";
  if (s == E::ERROR_IOCTL_I2C_RDWR_FAILURE)
    return "Error: IOCTL I2C RDWR failure";
  if (s == E::ERROR_PROG_BUF_CHECKSUM_ERROR)
    return "Error: Program buf checksum failure";
  if (s == E::ERROR_PROG_READ_CHECKSUM_ERROR)
    return "Error: Program read checksum failure";
  if (s == E::ERROR_PROG_OVER_THREE_TIMES)
    return "Error: Program failed over three times ";
  if (s == E::ERROR_ENTER_BOOTLOADER)
    return "Error: Entering Bootloader Failed";
  if (s == E::ERROR_TRANS_BLOCK)
    return "Error: Transfer Block";
  if (s == E::ERROR_TRANS_PAGE)
    return "Error: Transfer Page";
  if (s == E::ERROR_UPG_FAIL)
    return "Error: Upgarde failure";
  if (s == E::ERROR_UNKNOW)
    return "Error: Unknown failure";

  return nullptr;
}

PSUSlave_T slave_hdlr[] = {
    // {PsuNo, SlaveAddr},
    {0, I2C_ADDR_0x80}, {1, I2C_ADDR_0x82}, {2, I2C_ADDR_0x84},
    {3, I2C_ADDR_0x80}, {4, I2C_ADDR_0x82}, {5, I2C_ADDR_0x84},
};

unsigned char FW_ID[11] = {0x32, 0x39, 0x30, 0x30, 0x31, 0x30,
                           0x36, 0x31, 0x44, 0x43, 0x45};

std::string psuId;

MCUPart_T MCU_hdlr[] = {
    // {SectionFlash, FwSize, MCU_ID, BYTE_PER_PAGE, BYTE_PER_PACKET, BOOT_STA,
    // APPCRC_STA/FLASH_STA, Delay_1(ms), Delay_2(ms), Delay_3(ms)},
    {0, PRIM_IMA_SIZE, MCU_ID_PRIM, 256, 16, 0x04, 0x40, 80, 80, 500},
    {1, SEC_IMA_SIZE, MCU_ID_SEC, 256, 16, 0x04, 0x40, 100, 100, 500},
    {2, COM_IMA_SIZE, MCU_ID_COM, 256, 16, 0x04, 0x40, 15, 15, 200},
};

class DeltaPsu : public I2c {
public:
  DeltaPsu() = delete;
  DeltaPsu(const DeltaPsu &) = delete;
  DeltaPsu &operator=(const DeltaPsu &) = delete;
  DeltaPsu(DeltaPsu &&) = delete;
  DeltaPsu &operator=(DeltaPsu &&) = delete;
  ~DeltaPsu() = default;
  DeltaPsu(const int &busfd, const int &slaveaddr)
      : fd(busfd), slave(slaveaddr) {}

  int unlockDevice(uint8_t *write_buffer, uint8_t *read_buffer) {
    int ret = 0;
    // 3. Unlock upgrade command with password... (Operation: 0xF0 0x0C MCU_ID
    // [ECD16010081]-ASCII PEC)  => retry 5
    VERBOSE << "#3 Unlock Upgrade..." << endl;
    memset(write_buffer, 0x00, MAX_BUFFER_SIZE);
    memset(read_buffer, 0x00, MAX_BUFFER_SIZE);
    write_buffer[0] = REG_FIRMWARE_UNLOCK;
    write_buffer[1] = 0x0C;
    write_buffer[2] = mcuID;
    memcpy(&write_buffer[3], &FW_ID[0], sizeof(FW_ID)); // ECD16010081
    I2c::calculate_PEC(write_buffer, slave, 14);
    ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 15, 0);
    if (ret)
      return ret;

    return EXIT_SUCCESS;
  }

  int checkUpgradeModeStatus(uint8_t *write_buffer, uint8_t *read_buffer) {
    int ret = 0;
    VERBOSE << "#5 Check Upgrade Mode Status..." << endl;
    memset(write_buffer, 0x00, MAX_BUFFER_SIZE);
    memset(read_buffer, 0x00, MAX_BUFFER_SIZE);
    write_buffer[0] = REG_BOOT_STATUS_MODE;
    ret = I2c::transfer(fd, PSU_READ, slave, write_buffer, read_buffer, 1, 1);
    if (ret)
      return ret;

    return EXIT_SUCCESS;
  }

  int setBootFlag(uint8_t *write_buffer, uint8_t *read_buffer) {
    int ret = 0;
    VERBOSE << "#4 Set Boot Flag..." << endl;
    memset(write_buffer, 0x00, MAX_BUFFER_SIZE);
    memset(read_buffer, 0x00, MAX_BUFFER_SIZE);
    write_buffer[0] = REG_BOOT_STATUS_MODE;
    write_buffer[1] = mcuID;
    write_buffer[2] = 0x01; // Enable the boot status mode
    I2c::calculate_PEC(write_buffer, slave, 3);
    ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 4, 0);
    if (ret)
      return ret;

    return EXIT_SUCCESS;
  }

  int resetBootFlag(uint8_t *write_buffer, uint8_t *read_buffer) {
    int ret = 0;
    VERBOSE << "#4 reset Boot Flag..." << endl;
    memset(write_buffer, 0x00, MAX_BUFFER_SIZE);
    memset(read_buffer, 0x00, MAX_BUFFER_SIZE);
    write_buffer[0] = REG_BOOT_STATUS_MODE;
    write_buffer[1] = mcuID;
    write_buffer[2] = 0x00; // Disable the boot status mode
    I2c::calculate_PEC(write_buffer, slave, 3);
    ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 4, 0);
    if (ret)
      return ret;

    return EXIT_SUCCESS;
  }

  int disableDeviceOutput(uint8_t *write_buffer, uint8_t *read_buffer) {
    int ret = 0;
    VERBOSE << "#2 Disable PSU output..." << endl;
    memset(write_buffer, 0x00, MAX_BUFFER_SIZE);
    memset(read_buffer, 0x00, MAX_BUFFER_SIZE);
    write_buffer[0] = 0x00;
    write_buffer[1] = 0x00;
    I2c::calculate_PEC(write_buffer, slave, 2);
    ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 3, 0);
    if (ret)
      return ret;

    memset(write_buffer, 0x00, MAX_BUFFER_SIZE);
    memset(read_buffer, 0x00, MAX_BUFFER_SIZE);
    write_buffer[0] = 0x01;
    write_buffer[1] = 0x00;
    I2c::calculate_PEC(write_buffer, slave, 2);
    ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 3, 0);
    if (ret)
      return ret;

    return EXIT_SUCCESS;
  }

  int enableDeviceOutput(uint8_t *write_buffer, uint8_t *read_buffer) {
    int ret = 0;
    VERBOSE << "Enable PSU output..." << endl;
    memset(write_buffer, 0x00, MAX_BUFFER_SIZE);
    memset(read_buffer, 0x00, MAX_BUFFER_SIZE);
    write_buffer[0] = 0x01;
    write_buffer[1] = 0x80;
    I2c::calculate_PEC(write_buffer, slave, 2);
    ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 3, 0);
    if (ret)
      return ret;

    return EXIT_SUCCESS;
  }

  int fwupdate(string imageFilename, int section) {
    cout << "fwupdate is called with Image=" << imageFilename.c_str()
         << " on section= " << section << endl;

    int fw_file = -1;
    struct stat st;
    int ret = 0; // V
    uint32_t i, j;
    uint8_t *fw_buf = NULL;
    uint32_t fw_crc16 = 0;
    uint32_t CRC16_Data = 0;
    uint8_t write_buffer[MAX_BUFFER_SIZE] = {0}; // 0604
    uint8_t read_buffer[MAX_BUFFER_SIZE] = {0};  // 0604
    uint32_t Page_Count = 0;                     //
    int page_index = 0;                          //
    int bytePerBlock = 0;                        //
    int blockIndex;                              //
    int Flag_BreakFlag = 0,
        Flag_SetUnlockBootflag = 0; // Use to exit the 2 for loop
    std::map<std::string, std::string> addData;
    Level level = Level::Informational;
    section_flash = section;
    for (int i = 0; i < MCU_MAX_NUM; i++) {

      VERBOSE << "SectionFlash: " << MCU_hdlr[i].SectionFlash << endl;
      if (section_flash == MCU_hdlr[i].SectionFlash) {
        mcuID = MCU_hdlr[i].McuID;
        VERBOSE << "MCU_ID: " << mcuID << endl;
        bytePerPage = MCU_hdlr[i].BytePerPage;
        bytePerPacket = MCU_hdlr[i].BytePerPacket;
        bootStat = MCU_hdlr[i].BootSTA;
        delay1 = MCU_hdlr[i].Delay1;
        delay2 = MCU_hdlr[i].Delay2;
        delay3 = MCU_hdlr[i].Delay3;
        break;
      }
    }

    VERBOSE << "#0 Check FW Version..." << endl;
    ret = fwversion();
    if (ret)
      goto exit;

    /* 1. Load FW image */
    fw_file = open(imageFilename.c_str(), O_RDONLY);
    VERBOSE << "Image- " << imageFilename.c_str() << endl;
    if (fw_file < 0) {
      cerr << __func__ << ":" << __LINE__ << "-"
           << "Error opening FW-image file: " << strerror(errno) << endl;
      ret = static_cast<int>(ErrorStatus::ERROR_OPEN_FIRMWARE);
      goto exit;
    }

    stat(imageFilename.c_str(), &st);
    /* Check FW image size */
    if (st.st_size <= 0) {
      cerr << __func__ << ":" << __LINE__ << "-"
           << statusToString(ErrorStatus::ERROR_WRONG_FIRMWARE) << endl;
      ret = static_cast<int>(ErrorStatus::ERROR_WRONG_FIRMWARE);
      goto exit;
    }
    /* CRC16 Checksum and assign parameters*/
    fw_buf = static_cast<uint8_t *>(malloc(st.st_size));
    if (fw_buf == NULL) {
      cerr << __func__ << ":" << __LINE__ << "-"
           << statusToString(ErrorStatus::ERROR_MALLOC_FAILURE) << endl;
      ret = static_cast<int>(ErrorStatus::ERROR_MALLOC_FAILURE);
      goto exit;
    }
    if (read(fw_file, fw_buf, st.st_size) == -1) {
      cerr << __func__ << ":" << __LINE__ << "-"
           << "Error: reading image file: " << strerror(errno) << endl;
      goto exit;
    }

    fw_crc16 = Crc::crc16(fw_buf, static_cast<uint32_t>(st.st_size));
    VERBOSE << "PSU FW CRC16 Check: " << std::hex << static_cast<int>(fw_crc16)
            << endl;
    CRC16_Data = fw_crc16;
    VERBOSE << "FW Image CRC16 Checksum: " << std::hex
            << static_cast<int>(CRC16_Data) << endl;

    // 3. Unlock upgrade command with password... (Operation: 0xF0 0x0C MCU_ID
    // [ECD16010081]-ASCII PEC)  => retry 5
    ret = unlockDevice(write_buffer, read_buffer);
    if (ret)
      goto exit;

    // Delay 500 ms
    usleep(DELAY_500_MS); // usleep = micro seconds
    // 5. Check/Read upgrade mode status (optional)... (0xF1)
    // 		!= upgrade mode => continue
    VERBOSE << "#5 Check Upgrade Mode Status..." << endl;
    ret = checkUpgradeModeStatus(write_buffer, read_buffer);
    if (ret)
      goto exit;
    VERBOSE << "Status: " << std::hex << static_cast<int>(read_buffer[0])
            << endl;
    if ((read_buffer[0] & bootStat) != bootStat) {
      /* 2. Turn off the PSU output... (Operation: 0x01 0x00) */
      ret = disableDeviceOutput(write_buffer, read_buffer);
      if (ret)
        goto exit;
    } else // Bit2=1? Yes!
    {
      // Skip Turn OFF, Unlock and Set Bootflag
      goto JumpToTransmit;
    }

    Flag_SetUnlockBootflag = 0; // Fail count for set "unlock" and "bootflag"
  SetUnlockBootflag:
    // 3. Unlock upgrade command with password... (Operation: 0xF0 0x0C MCU_ID
    // [ECD16010081]-ASCII PEC)  => retry 5
    ret = unlockDevice(write_buffer, read_buffer);
    if (ret)
      goto exit;

    // Delay 500 ms
    usleep(DELAY_500_MS); // usleep = micro seconds

    // 4. Set Boot flag... (Operation: 0xF1 MCU_ID 0x01)
    ret = setBootFlag(write_buffer, read_buffer);
    if (ret)
      goto exit;

    // Delay 5 s
    usleep(DELAY_5_S); // usleep = micro seconds

    // 5. Check/Read upgrade mode status (optional)... (0xF1)
    // 		!= upgrade mode => continue
    ret = checkUpgradeModeStatus(write_buffer, read_buffer);
    if (ret)
      goto exit;
    VERBOSE << "Status: " << std::hex << static_cast<int>(read_buffer[0])
            << endl;
    if ((read_buffer[0] & bootStat) != bootStat) // Bit2=1? No!
    {
      Flag_SetUnlockBootflag++;
      if (Flag_SetUnlockBootflag < 10)
        goto SetUnlockBootflag;
      else {
        ret = static_cast<int>(ErrorStatus::ERROR_ENTER_BOOTLOADER);
        cerr << __func__ << ":" << __LINE__ << "-"
             << statusToString(ErrorStatus::ERROR_ENTER_BOOTLOADER) << endl;
        goto exit;
      }
    }

    // 3. Unlock upgrade command with password... (Operation: 0xF0 0x0C MCU_ID
    // [ECD16010081]-ASCII PEC)  => retry 5
    ret = unlockDevice(write_buffer, read_buffer);
    if (ret)
      goto exit;

    // Delay 500 ms
    usleep(DELAY_500_MS); // usleep = micro seconds

  JumpToTransmit: // Jump to here after Skip Turn OFF, Unlock and Set Bootflag
    addData["REDFISH_MESSAGE_ID"] = transferringToComponent;
    addData["REDFISH_MESSAGE_ARGS"] = (imageFilename + "," + "PSU" + psuId);
    addData["namespace"] = "FWUpdate";
    level = Level::Informational;
    createLog(transferringToComponent, addData, level);

    bytePerBlock = static_cast<int>((bytePerPage / bytePerPacket)); // 256 / 16
    Page_Count = (static_cast<uint32_t>(st.st_size) / bytePerPage);

    for (i = 0; i < Page_Count; i++) {

      for (blockIndex = 0; blockIndex < bytePerBlock; blockIndex++) {
        /* 6. Transmit 16 x 16 Bytes into RAM (Operation: 0xF2 0x13 MCU_ID
         * Block_LO Block_HI Data[16] PEC) */
        int Flag_RepeatTrans = 0; // mv
      RepeatTrans:                // mv
        VERBOSE << "#6 Transmit into RAM... " << endl;
        memset(write_buffer, 0x00, sizeof(write_buffer));
        memset(read_buffer, 0x00, sizeof(read_buffer));
        write_buffer[0] = REG_TRANSMIT_RAM;
        write_buffer[1] =
            static_cast<uint8_t>((bytePerPacket + 3)); // Byte count
        write_buffer[2] = mcuID;
        write_buffer[3] = static_cast<uint8_t>((blockIndex)); // Block_LO
        write_buffer[4] = 0x00;                               // Block_HI

        uint8_t *block =
            static_cast<uint8_t *>(malloc(bytePerPacket * sizeof(uint8_t)));
        memset(block, 0x00, bytePerPacket);
        memset(read_buffer, 0x00, sizeof(read_buffer));
        for (j = 0; j < bytePerPacket; j++) {
          block[j] = fw_buf[(i * 256) + (blockIndex * 16) + j];
        }

        memcpy(&write_buffer[5], &block[0], bytePerPacket);
        I2c::calculate_PEC(write_buffer, slave, 21);
        if (Flag_RepeatTrans < 10) {
          ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 22, 0);
          if (ret)
            goto transferFail;

          // Delay 1
          usleep(delay1 * DELAY_MS);

          // 5. Check upgrade mode status ... (0xF1)
          ret = checkUpgradeModeStatus(write_buffer, read_buffer);
          VERBOSE << "Status: " << std::hex << static_cast<int>(read_buffer[0])
                  << endl;
          if ((read_buffer[0] & TRANS_ERR) == TRANS_ERR) {
            cerr << __func__ << ":" << __LINE__ << "-"
                 << "#6 Programming 'Transmit' status failed: " << TRANS_ERR
                 << endl;
            Flag_RepeatTrans++;
            goto RepeatTrans;
          }
        } else {
          // Break out the "Block count" loop
          Flag_BreakFlag = 1;
          ret = static_cast<int>(ErrorStatus::ERROR_TRANS_BLOCK);
          cerr << __func__ << ":" << __LINE__ << "-"
               << statusToString(ErrorStatus::ERROR_TRANS_BLOCK) << endl;
          break;
        }
      }

      if (Flag_BreakFlag != 0) {
        break; // Break out the "Page count" loop for Block loop
               // breaking!!!!
      }

      /* 7. Write RAM Data into FLASH (256 bytes) (Operation: 0xF3 0x03(byte
       * count) MCU_ID PageAddr_LO PageAddr_HI PEC) */
      VERBOSE << "#7 Write/Commit RAM Data into Flash... " << endl;
      memset(write_buffer, 0x00, sizeof(write_buffer));
      memset(read_buffer, 0x00, sizeof(read_buffer));
      write_buffer[0] = REG_TRANSMIT_FLASH;
      write_buffer[1] = 0x03;
      write_buffer[2] = mcuID;
      write_buffer[3] = static_cast<uint8_t>(i); // PageAddr_LO
      write_buffer[4] = 0x00;                    // PageAddr_HI
      I2c::calculate_PEC(write_buffer, slave, 5);

      int Flag_RepeatWrite = 0;
    RepeatWrite:
      if (Flag_RepeatWrite < 10) {
        ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 6, 0);
        if (ret) {
          Flag_RepeatWrite++;
          goto RepeatWrite;
        }

        if (MCU_ID_PRIM == mcuID) {
          if ((0 == i) || (0 == (i % 8))) {
            // Delay 3
            usleep(delay3 * DELAY_MS);
          } else {
            // Delay 2
            usleep(delay2 * DELAY_MS);
          }
        } else {
          if ((0 == i) || (0 == (i % 4))) {
            // Delay 3
            usleep(delay3 * DELAY_MS);
          } else {
            // Delay 2
            usleep(delay2 * DELAY_MS);
          }
        }

        // Check upgrade mode status ... (0xF1)
        ret = checkUpgradeModeStatus(write_buffer, read_buffer);
        VERBOSE << "Status: " << std::hex << static_cast<int>(read_buffer[0])
                << endl;
        if ((read_buffer[0] & TRANS_ERR) == TRANS_ERR) {
          cerr << __func__ << ":" << __LINE__ << "-"
               << "#6 Programming 'Transmit' status failed: " << TRANS_ERR
               << endl;
          Flag_RepeatWrite++;
          goto RepeatWrite;
        }
      } else {
        // Break out the "Page count" loop
        Flag_BreakFlag = 1;
        cerr << __func__ << ":" << __LINE__ << "-"
             << "Fail to Transmission Page Data" << endl;
        ret = static_cast<int>(ErrorStatus::ERROR_TRANS_PAGE);
        break;
      }

      /* Progressing */
      page_index = page_index + 1;
      cout << "Upgrade Progress... " << std::dec
           << (page_index * 99 / Page_Count) << endl;

    } // End of for(i = 0; i < Page_Count; i++)

    if (Flag_BreakFlag != 0) {
      goto exit;
    }

    /* 8. Transmit CRC16 (Operation: 0xF4 0x03(byte count) MCU_ID CRC16_LO
     * CRC16_HI PEC) */
    VERBOSE << "#8 Write CRC16..." << endl;
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_buffer, 0x00, sizeof(read_buffer));
    write_buffer[0] = REG_TRANSMIT_CRC;
    write_buffer[1] = 0x03;
    write_buffer[2] = mcuID;
    write_buffer[3] = static_cast<uint8_t>(CRC16_Data & 0xFF);
    write_buffer[4] = static_cast<uint8_t>((CRC16_Data >> 8) & 0xFF);
    VERBOSE << "Re-Check FW Image CRC16 Checksum before transmit: "
            << CRC16_Data << endl;
    VERBOSE << "write_buffer[3]: " << std::hex
            << static_cast<int>(write_buffer[3]) << endl;
    VERBOSE << "write_buffer[4]: " << std::hex
            << static_cast<int>(write_buffer[4]) << endl;
    I2c::calculate_PEC(write_buffer, slave, 5);
    ret = I2c::transfer(fd, PSU_WRITE, slave, write_buffer, 0, 6, 0);
    if (ret)
      goto exit;

    // Delay 1s
    usleep(DELAY_1_S); // usleep = micro seconds

    /* 9. Reset Boot flag (Operation: 0xF1 MCU_ID 0x00) */
    ret = resetBootFlag(write_buffer, read_buffer);
    if (ret)
      goto exit;

    // Delay 5s
    usleep(DELAY_5_S); // usleep = micro seconds

    // 5. Check/Read upgrade mode status (optional)... (0xF1)  <1>
    // 	 != upgrade mode => continue
    ret = checkUpgradeModeStatus(write_buffer, read_buffer);
    if (ret)
      goto exit;
    VERBOSE << "Status: " << std::hex << static_cast<int>(read_buffer[0])
            << endl;
    if ((read_buffer[0] & bootStat) != bootStat) // Bit2=0? Yes!
    {
      if (mcuID != MCU_ID_COM) {
        /* Turn on the PSU output... (Operation: 0x01 0x80) */
        ret = enableDeviceOutput(write_buffer, read_buffer);
      }
      VERBOSE << "FW Upgrade Complete for slave " << slave << endl;
    } else {
      ret = static_cast<int>(ErrorStatus::ERROR_UPG_FAIL);
      cerr << __func__ << ":" << __LINE__ << "-"
           << statusToString(ErrorStatus::ERROR_UPG_FAIL) << endl;
      goto exit;
    }
    VERBOSE << "#0 Check FW Version... after upgrade command " << endl;
    I2c::calculate_PEC(write_buffer, slave, 14);
    ret = fwversion();
    goto exit;

  transferFail:
    addData["REDFISH_MESSAGE_ID"] = transferFailed;
    addData["REDFISH_MESSAGE_ARGS"] = (imageFilename + "," + "PSU" + psuId);
    addData["namespace"] = "FWUpdate";
    level = Level::Critical;
    createLog(transferFailed, addData, level);

  exit:
    if (fw_buf) {
      free(fw_buf);
    }
    if (fw_file != -1) {
      close(fw_file);
      fw_file = -1;
    }
    return ret;
  }
  int fwversion() {
    int ret = 0;
    VERBOSE << "fwversion is called" << endl;
    uint8_t write_buffer[1024] = {0}; // 0604
    uint8_t read_buffer[1024] = {0};  // 0604
    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_buffer, 0x00, sizeof(read_buffer));
    write_buffer[0] = 0xE2;
    cout << "Version:slave " << slave << endl;
    I2c::calculate_PEC(write_buffer, slave, 14);
    ret = I2c::transfer(fd, PSU_READ, slave, write_buffer, read_buffer, 1, 7);
    if (ret)
      return ret;
    cout << " Version:Byte Count " << static_cast<int>(read_buffer[0]) << endl;
    cout << "Version:Comm Major Rev " << std::hex
         << static_cast<int>(read_buffer[1]) << " Minor Rev " << std::hex
         << static_cast<int>(read_buffer[2]) << endl;
    cout << "Version:Sec Major Rev " << std::hex
         << static_cast<int>(read_buffer[3]) << " Minor Rev " << std::hex
         << static_cast<int>(read_buffer[4]) << endl;
    cout << "Version:Prim  Major Rev " << std::hex
         << static_cast<int>(read_buffer[5]) << " Minor Rev " << std::hex
         << static_cast<int>(read_buffer[6]) << endl;

    return 0;
  }
  int activate() {
    VERBOSE << "activate is called" << endl;
    return 0;
  }

private:
  int fd;
  int slave;
  uint32_t section_flash;
  int flashing_progress;
  uint32_t bootStat = 0;
  uint8_t mcuID = 0;
  uint32_t bytePerPacket = 0;
  uint32_t bytePerPage = 0;
  uint32_t delay1 = 0, delay2 = 0, delay3 = 0;
};
