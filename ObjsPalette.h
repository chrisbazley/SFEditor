/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground objects palette
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef ObjsPalette_h
#define ObjsPalette_h

#include <stdbool.h>

struct PaletteData;
bool ObjsPalette_register(struct PaletteData *palette);

enum {
  /* The two least-significant bits of the cloud type control its colour
     (in conjunction with its address, which we can't predict) */
  ObjsPaletteNumHills = 1,
};

#endif
