/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Selected ground objects properties dialogue box
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef OPropDbox_h
#define OPropDbox_h

#include <stdbool.h>
#include "toolbox.h"

#include "MapCoord.h"
#include "Triggers.h"
#include "IntDict.h"

struct EditWin;
struct Editor;

typedef struct
{
  struct Editor *editor;
  IntDict sa_coords;
  IntDict sa_window;
} ObjPropDboxes;

void ObjPropDboxes_init(ObjPropDboxes *prop_dboxes, struct Editor *editor);

void ObjPropDboxes_destroy(ObjPropDboxes *prop_dboxes);
void ObjPropDboxes_update_title(ObjPropDboxes *prop_dboxes);

void ObjPropDboxes_update_for_move(ObjPropDboxes *prop_dboxes,
                                   MapPoint old_pos,
                                   MapPoint new_pos);

void ObjPropDboxes_update_for_del(ObjPropDboxes *prop_dboxes,
                                  MapArea const *bbox);

void ObjPropDboxes_open(ObjPropDboxes *prop_dboxes, MapPoint pos,
                        struct EditWin *edit_win);

bool ObjPropDboxes_drag_obj_link(ObjPropDboxes *prop_dboxes,
  int window, int icon, MapPoint pos);

#endif
