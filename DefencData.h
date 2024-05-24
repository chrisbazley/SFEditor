/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission ground defences data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef DefencData_h
#define DefencData_h

#include "Ships.h"

struct DefencesData
{
  int timer;
  ShipType ship_type;
  unsigned char fire_prob;
  unsigned char laser_type;
  unsigned char ships_per_hangar;
  unsigned char ship_prob;
};

#endif
