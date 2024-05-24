/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission big ships performance data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef BPerfData_h
#define BPerfData_h

#include "Ships.h"

struct BigPerform {
  int shields;
  int laser_prob;
  int missile_prob;
  int ship_prob;
  unsigned char laser_type;
  unsigned char ship_count;
  bool remote_shield;
};

struct BigPerformData {
  struct BigPerform type[ShipType_Big3 - ShipType_Big1 + 1];
};

#endif
