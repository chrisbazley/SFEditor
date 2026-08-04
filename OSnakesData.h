/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object snakes data
 *  Copyright (C) 2021 Chris Bazley
 */

#ifndef OSnakesData_h
#define OSnakesData_h

#include "SnakesData.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

struct ObjSnakes
{
  struct Snakes super;
  _Optional long int *distances;
};

#endif
