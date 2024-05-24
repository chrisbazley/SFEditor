/*
 * SFeditor - Star Fighter 3000 map/mission editor
 * Polygonal object plot groups
 * Copyright (C) 2021 Christopher Bazley
 */

#ifndef OBJGROUP_H
#define OBJGROUP_H

#include "SFError.h"
struct Reader;

#include "ObjPolygon.h"

typedef struct {
  int npolygons;
  void *polygons;
} ObjGroup;

void obj_group_init(ObjGroup *group);
void obj_group_free(ObjGroup *group);

SFError obj_group_add_polygon(ObjGroup *group, ObjPolygon polygon);
ObjPolygon obj_group_get_polygon(ObjGroup *group, int n);
int obj_group_get_count(ObjGroup *group);

#endif /* OBJGROUP_H */
