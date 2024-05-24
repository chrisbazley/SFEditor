/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map snakes data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef MSnakesData_h
#define MSnakesData_h

#include <stdbool.h>
#include "SprMem.h"
#include "SnakesData.h"

struct MapSnakes
{
  struct Snakes super;
  SprMem thumbnail_sprites; /* flex anchor for sprite area */
  bool have_thumbnails;
};

#endif
