#include <algorithm>
#include <cstdint> // uint8_t (8 bits) and uint16_t (16 bits)
#include <cstdlib> // <stdlib.h>
#include <cstring> // <string.h>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

//------ Poxix/ U-nix specific header files ------//
#include "deltapsuxx.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_RETRY 3
#define DELAY_100_US 100

int file_lock(int fd) {
  /* Lock the file from beginning to end, blocking if it is already locked */
  struct flock lock, savelock;
  lock.l_type = F_WRLCK; /* Test for any lock on any part of file. */
  lock.l_start = 0;
  lock.l_whence = SEEK_SET;
  lock.l_len = 0;
  savelock = lock;
  fcntl(fd, F_GETLK, &lock); /* Overwrites lock structure with preventors. */
  if (lock.l_type == F_WRLCK) {
    cerr << "Process " << lock.l_pid << " has a write lock already!" << endl;
    exit(1);
  } else if (lock.l_type == F_RDLCK) {
    cerr << "Process " << lock.l_pid << " has a read lock already!" << endl;
    exit(1);
  }
  return fcntl(fd, F_SETLK, &savelock);
}

int file_unlock(int fd) {

  struct flock lock, savelock;
  lock.l_type = F_UNLCK; /* Test for any lock on any part of file. */
  lock.l_start = 0;
  lock.l_whence = SEEK_SET;
  lock.l_len = 0;
  savelock = lock;

  fcntl(fd, F_GETLK, &lock); /* Overwrites lock structure with preventors. */
  if (lock.l_type == F_WRLCK || lock.l_type == F_RDLCK) {
    cerr << "Process " << lock.l_pid << " has a write lock already!" << endl;
    return fcntl(fd, F_SETLK, &savelock);
  } else {
    cerr << "Process " << lock.l_pid << " lock.l_type - " << lock.l_type
         << " has not been locked !" << endl;
    return 0;
  }
}

//---------- Main Code -------------------------------//
#ifdef STANDALONE_UTILITY
int main(int argc, char **argv) {
  const char *programName = argv[0];
  int bus, device_slaveaddr, device_section;
  string imageFilename, cmd;
  int fd = -1, i = 0, retry = 0, ret = 0;
  auto printUsage = [programName]() {
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, " %s version [bus] [slave address]  [PSU section]  \n",
            programName);
    fprintf(stderr,
            " %s fwupgrade [bus] [slave address] [PSU section] [image file] \n",
            programName);
    fprintf(stderr, " %s activate [bus] [slave address]  [PSU section]\n",
            programName);
    fprintf(stderr, " options:\n");
    fprintf(stderr, " -v  verbose\n");
  };

  int opt;

  // put ':' in the starting of the
  // string so that program can
  // distinguish between '?' and ':'
  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
    case 'v':
      verbose = 1;
      VERBOSE << "Verbose mode started" << endl;
      break;
    }
  }

  if (optind >= argc) {
    printUsage();
    cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
    return static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS);
  }

  VERBOSE << "argc=" << argc << endl;
  VERBOSE << "optind=" << optind << endl;
  if (argc < 5) {
    printUsage();
    cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
    return static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS);
  }
  cmd = string(argv[optind]);
  bus = stoi(argv[optind + 1], nullptr, 0);
  device_slaveaddr = stoi(argv[optind + 2], nullptr, 0);
  device_section = stoi(argv[optind + 3], nullptr, 0);

#else
int componentSendCommand(string cmd, int bus, int device_slaveaddr,
                         int device_section, string imageFilename) {
  int fd = -1, i = 0, retry = 0, ret = 0;
#endif
  fd = I2c::openBus(bus);
  if (fd < 0) {
    cout << "Error opening i2c file: " << strerror(errno) << endl;
    return static_cast<int>(ErrorStatus::ERROR_OPEN_I2C_DEVICE);
  }
  for (i = 0; i < MAX_RETRY; i++) {
    /* Get shared lock */
    if ((ret = file_lock(fd)) >= 0) {
      VERBOSE << "Process got a read lock already!  " << endl;
      break;
    } else if (i == MAX_RETRY) {
      cerr << __func__ << ":" << __LINE__ << "-"
           << "ERROR: Getting lock Failed!!!" << endl;
      close(fd);
      return EXIT_FAILURE;
    }
    usleep(DELAY_100_US);
  }
  VERBOSE << "[bus]= " << bus << " [slave address]= " << device_slaveaddr
          << " [PSU section]= " << device_section << endl;
  DeltaPsu device(fd, device_slaveaddr, device_section);
  if (cmd == "version") {
    VERBOSE << "Retriving version number" << endl;
    device.fwversion();
  } else if (cmd == "fwupgrade") {
#ifdef STANDALONE_UTILITY
    VERBOSE << "argc=" << argc << endl;
    if (argc < 6) {
      printUsage();
      cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
      file_unlock(fd);
      close(fd);
      return static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS);
    }
    imageFilename = argv[optind + 4];
#endif
    do {
      VERBOSE << "starting fwupgrade process " << endl;
      ret = device.fwupdate(imageFilename.c_str());
      if (ret == 0) {
        break;
      } else if (retry > MAX_RETRY) {
        VERBOSE << "retry= " << retry << endl;
        ret = static_cast<int>(ErrorStatus::ERROR_UPG_FAIL);
        break;
      } else {

        retry++;
      }
    } while (1);
  } else if (cmd == "activate") {
    VERBOSE << "Activate the device" << endl;
    device.activate();
  } else {
#ifdef STANDALONE_UTILITY
    printUsage();
    cout << statusToString(ErrorStatus::ERROR_INPUT_ARGUMENTS) << endl;
#endif
    file_unlock(fd);
    close(fd);
    return static_cast<int>(ErrorStatus::ERROR_INPUT_ARGUMENTS);
  }
  if (fd != -1) {
    file_unlock(fd);
    close(fd);
  }

  return EXIT_SUCCESS;
} // --- End of main() --- //
