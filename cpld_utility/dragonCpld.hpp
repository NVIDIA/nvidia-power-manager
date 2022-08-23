#include "dragonChassisBase.hpp"

#include <stdint.h>

class DragonCpld : public DragonChassisBase {
public:
  DragonCpld(int updateBus, bool arbitrator, char *imageName,
             const char *config);
  virtual ~DragonCpld();
  int fwUpdate();

protected:
  int cpldRegBus;
  int cpldRegAddress;
  int cpldRegFd;
  int cpldRawBus;
  const char *config;

  int enableConfigurationInterface();
  int erase(bool reset);
  int activate();
  int cleanUp();
  int waitBusy(int wait, int errorCode);
  int readStatusRegister(uint32_t *value);
  int waitRefresh(int result, int fd);
  int loadConfig();
  int sendImage() override;
};
