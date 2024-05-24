/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission fighter performance data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef FPerfData_h
#define FPerfData_h

#include "Ships.h"

struct FighterPerform {
  int shields;
  int engine;
  int laser_prob;
  int missile_prob;
  unsigned char laser_type;
  unsigned char control;
};

struct FighterPerformData {
  struct FighterPerform type[ShipType_Fighter4 - ShipType_Fighter1 + 1];
};

#endif
