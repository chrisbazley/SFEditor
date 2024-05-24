/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map area collection
 *  Copyright (C) 2023 Christopher Bazley
 */

#ifndef MapAreaCol_h
#define MapAreaCol_h

#include <stddef.h>
#include "MapCoord.h"

typedef struct MapAreaColData MapAreaColData;

void MapAreaCol_init(MapAreaColData *coll, int const size_log2);
void MapAreaCol_add(MapAreaColData *coll, MapArea const *area);

typedef struct
{
  MapAreaColData const *coll;
  size_t next;
}
MapAreaColIter;

MapArea const *MapAreaColIter_get_first(MapAreaColIter *iter, MapAreaColData const *coll);
MapArea const *MapAreaColIter_get_next(MapAreaColIter *iter);

#endif
