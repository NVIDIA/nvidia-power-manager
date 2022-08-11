#include "dragonChassisBase.hpp"

#include <stdint.h>

class DragonCpld : public DragonChassisBase {
public:
  DragonCpld(int updateBus, int updateAddress, bool arbitrator, char *imageName,
             int cpldRegBus, int cpldRegAddress);
  virtual ~DragonCpld();
  int fwUpdate();

protected:
  int cpldRegBus;
  int cpldRegAddress;
  int cpldRegFd;

  int enableConfigurationInterface();
  int erase();
  int activate();
  int waitBusy(int wait, int errorCode);
  int readStatusRegister(uint32_t *value);

  int sendImage() override;
};
