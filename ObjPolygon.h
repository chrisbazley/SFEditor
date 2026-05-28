/*
 * SFeditor - Star Fighter 3000 map/mission editor
 * Polygonal object polygons
 * Copyright (C) 2021 Christopher Bazley
 */

#ifndef OBJPOLYGON_H
#define OBJPOLYGON_H

#include "SFError.h"
struct Reader;
#include "Debug.h"

enum {
  ObjPolygonMinSides = 3,
  ObjPolygonMaxSides = 15,
  ObjPolygonMaxGroups = 8,
  ObjPolygonFacingCheckGroup = 7,
};

typedef struct {
  unsigned short colour;
  unsigned char group;
  unsigned char scount;
  unsigned char sides[ObjPolygonMaxSides];
} ObjPolygon;

typedef struct {
  int pcount, palloc;
  void *polygons;
} ObjGroup;

typedef struct {
  ObjGroup groups[ObjPolygonMaxGroups];
} ObjPolygons;

void obj_polygons_init(ObjPolygons *polygons);
void obj_polygons_free(ObjPolygons *polygons);

SFError obj_polygons_read(ObjPolygons *polygons, struct Reader *reader,
  int num_vertices, int *max_group);

ObjGroup *obj_polygons_get_group(ObjPolygons *polygons, int n);

int obj_group_get_polygon_count(ObjGroup *group);

ObjPolygon obj_group_get_polygon(ObjGroup *group, int n);

static inline int obj_polygon_get_side(ObjPolygon const * const polygon, const int n)
{
  assert(polygon != NULL);
  assert(polygon->scount >= ObjPolygonMinSides);
  assert(polygon->scount <= ObjPolygonMaxSides);
  assert(n < polygon->scount);

  return polygon->sides[n];
}

static inline int obj_polygon_get_side_count(ObjPolygon const * const polygon)
{
  assert(polygon != NULL);
  assert(polygon->scount >= ObjPolygonMinSides);
  assert(polygon->scount <= ObjPolygonMaxSides);
  return polygon->scount;
}

static inline int obj_polygon_get_colour(ObjPolygon const * const polygon)
{
  assert(polygon != NULL);
  return polygon->colour;
}

#endif /* OBJPOLYGON_H */
