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
  ShipType ship_type;
  int timer, fire_prob, laser_type, ships_per_hangar, ship_prob;
};

#endif
