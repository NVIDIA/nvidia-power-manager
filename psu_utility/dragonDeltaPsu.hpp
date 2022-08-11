#include "dragonChassisBasePsu.hpp"

class DragonDeltaPsu : public DragonChassisBasePsu {
public:
  DragonDeltaPsu(int bus, int address, bool arbitrator,
                 const char *fwIdentifier, char *imageName);
  virtual ~DragonDeltaPsu();

private:
  char fwIdent[15];

  virtual int unlock() override;
  virtual int erase() override;
  virtual int sendImage() override;
  virtual int activate() override;
  int checkStatus(char *read);
  int sendCrc(uint16_t crc, uint16_t blockSize);
  int readCrc(uint16_t crc, uint16_t blockSize);
  void printFwVersion() override;
};
