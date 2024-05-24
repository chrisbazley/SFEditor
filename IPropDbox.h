/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information properties dialogue box
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef IPropDbox_h
#define IPropDbox_h

#include "MapCoord.h"
#include "IntDict.h"

struct EditWin;
struct Editor;
struct TargetInfo;

typedef struct
{
  struct Editor *editor;
  IntDict sa;
} InfoPropDboxes;

void InfoPropDboxes_init(InfoPropDboxes *prop_dboxes, struct Editor *editor);

void InfoPropDboxes_destroy(InfoPropDboxes *prop_dboxes);
void InfoPropDboxes_update_title(InfoPropDboxes *prop_dboxes);

void InfoPropDboxes_update_for_move(InfoPropDboxes *prop_dboxes,
                                    struct TargetInfo const *info,
                                    MapPoint old_pos);

void InfoPropDboxes_update_for_del(InfoPropDboxes *prop_dboxes,
                                   struct TargetInfo const *info);

void InfoPropDboxes_open(InfoPropDboxes *prop_dboxes, struct TargetInfo *info,
                         struct EditWin *edit_win);

#endif
