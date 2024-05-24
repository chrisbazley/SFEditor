/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Shape rasterisation
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef Shapes_h
#define Shapes_h

#include "MapCoord.h"

typedef size_t ShapesReadFunction(MapPoint, void *);
typedef void ShapesWriteFunction(MapArea const *, void *);

void Shapes_tri(ShapesWriteFunction *write, void *arg,
  MapPoint vertex_A, MapPoint vertex_B, MapPoint vertex_C);

void Shapes_rect(ShapesWriteFunction *write, void *arg,
  MapPoint vertex_A, MapPoint vertex_B);

void Shapes_circ(ShapesWriteFunction *write, void *arg,
  MapPoint centre, MapCoord radius);

void Shapes_line(ShapesWriteFunction *write, void *arg,
  MapPoint start, MapPoint end, MapCoord thickness);

bool Shapes_flood(ShapesReadFunction *read,
  ShapesWriteFunction *write, void *arg,
  size_t find, MapPoint centre,
  MapCoord limit);

#endif
