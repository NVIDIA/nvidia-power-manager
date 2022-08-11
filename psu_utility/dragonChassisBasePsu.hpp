#include "dragonChassisBase.hpp"

#include <stdint.h>

class DragonChassisBasePsu : public DragonChassisBase {
public:
  DragonChassisBasePsu(int bus, int address, bool arbitrator, char *imageName,
                       const char *manufacturer, const char *model);
  virtual ~DragonChassisBasePsu();
  int fwUpdate();

protected:
  const char *manufacturer;
  const char *model;

  int checkManufacturer();
  virtual int unlock() = 0;
  virtual int erase() = 0;
  virtual int activate() = 0;
  virtual void printFwVersion() = 0;
};
