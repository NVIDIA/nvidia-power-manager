#include "dragonChassisBase.hpp"
#include <stdint.h>
#include <gpiod.hpp>

class DragonCpld : public DragonChassisBase {
public:
  DragonCpld(int updateBus, char *imageName,
             const char *config);
  virtual ~DragonCpld();
  int fwUpdate();

protected:
  int cpldRegBus;
  int cpldRegAddress;
  int cpldRegFd;
  int cpldRawBus;
  const char *config;
  gpiod::line cpldRefreshGpio;

  int enableConfigurationInterface();
  int erase(bool reset);
  int activate();
  int readDeviceId();
  int cleanUp();
  int waitBusy(int wait, int errorCode);
  int readStatusRegister(uint32_t *value);
  int waitRefresh(int result);
  int loadConfig();
  int sendImage() override;
};
