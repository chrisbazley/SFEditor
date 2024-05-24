/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map rename dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef RenameMap_h
#define RenameMap_h

#include "toolbox.h"
#include "FilenamesData.h"

extern ObjectId RenameMap_id;

void RenameMap_created(ObjectId dbox_id);
void RenameMap_get_path(ComponentId component, Filename *file_path);
void RenameMap_set_path(ComponentId component, char *file_path);
char const *RenameMap_get_popup_title(ComponentId component);
#endif
