#ifndef CARCAR_SENSORS_H
#define CARCAR_SENSORS_H

namespace Sensors {
struct Values {
  int l3;
  int l2;
  int m;
  int r2;
  int r3;
};

void InitPins();
void Read();
Values Current();
} // namespace Sensors

#endif
