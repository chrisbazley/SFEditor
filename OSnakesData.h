/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object snakes data
 *  Copyright (C) 2021 Chris Bazley
 */

#ifndef OSnakesData_h
#define OSnakesData_h

#include "SnakesData.h"

struct ObjSnakes
{
  struct Snakes super;
  long int *distances;
};

#endif
