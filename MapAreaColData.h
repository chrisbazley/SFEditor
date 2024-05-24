/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Private data for map area collection
 *  Copyright (C) 2023 Christopher Bazley
 */

#ifndef MapAreaColData_h
#define MapAreaColData_h

#include "MapCoord.h"
#include <stdbool.h>

enum {
  MapAreaColMax = 8,
};

typedef struct {
  MapArea bbox;
  MapCoord area;
} MapAreaColEntry;

struct MapAreaColData {
  size_t count;
  int size_log2;
  MapAreaColEntry areas[MapAreaColMax];
};

#endif
