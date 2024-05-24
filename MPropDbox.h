/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Selected map area properties dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MPropDbox_h
#define MPropDbox_h

#include "toolbox.h"

#include "MapCoord.h"
#include "MapAnims.h"
#include "IntDict.h"
#include "Map.h"

struct EditWin;
struct Editor;

typedef struct
{
  struct Editor *editor;
  IntDict sa;
} MapPropDboxes;

void MapPropDboxes_init(MapPropDboxes *prop_dboxes, struct Editor *editor);

void MapPropDboxes_destroy(MapPropDboxes *prop_dboxes);
void MapPropDboxes_update_title(MapPropDboxes *prop_dboxes);

void MapPropDboxes_update_for_move(MapPropDboxes *prop_dboxes,
                                   MapPoint old_pos,
                                   MapPoint new_pos);

void MapPropDboxes_update_for_del(MapPropDboxes *prop_dboxes,
                                  MapArea const *bbox);

void MapPropDboxes_open(MapPropDboxes *prop_dboxes, MapPoint pos,
                        struct EditWin *edit_win);

#endif
