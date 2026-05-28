/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object snakes tool implementation
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef OSnakes_h
#define OSnakes_h

#include <stdbool.h>

#include "MapCoord.h"
#include "Snakes.h"
#include "Obj.h"

typedef struct ObjSnakes ObjSnakes;

struct EditSession;
struct ObjGfxMeshes;
struct ObjEditChanges;
struct ObjEditContext;

typedef struct {
  SnakeContext super;
  struct EditSession *session;
  struct ObjEditContext const *objects;
  struct ObjEditChanges *change_info;
  struct ObjGfxMeshes *meshes;
} ObjSnakesContext;

void ObjSnakes_init(ObjSnakes *snakes_data);

int ObjSnakes_get_count(const ObjSnakes *snakes_data);

void ObjSnakes_get_name(const ObjSnakes *snakes_data, int snake,
  char *snake_name, int n);

long int ObjSnakes_get_pal_distance(const ObjSnakes *snakes_data, int snake);

void ObjSnakes_set_pal_distance(ObjSnakes *snakes_data, int snake, long int distance);

void ObjSnakes_edit(const char *tiles_set);

void ObjSnakes_load(ObjSnakes *snakes_data,
  const char *tiles_set, int ntiles);

void ObjSnakes_begin_line(ObjSnakesContext *ctx,
  struct EditSession *session,
  ObjSnakes *snakes_data, MapPoint map_pos, int snake,
  bool inside, struct ObjEditChanges *change_info,
  struct ObjGfxMeshes *meshes);

void ObjSnakes_plot_line(ObjSnakesContext *ctx, MapPoint map_pos,
  struct ObjEditChanges *change_info);

void ObjSnakes_free(ObjSnakes *snakes_data);

ObjRef ObjSnakes_get_value(struct EditSession *session,
  ObjSnakes *snakes_data, MapPoint map_pos, int snake,
  bool inside, struct ObjGfxMeshes *meshes);

#endif
