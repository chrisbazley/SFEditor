/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission rename dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef RenameMiss_h
#define RenameMiss_h

#include "toolbox.h"
#include "FilenamesData.h"

extern ObjectId RenameMiss_id;

void RenameMiss_created(ObjectId dbox_id);
void RenameMiss_get_path(ComponentId component, Filename *file_path);
void RenameMiss_set_path(ComponentId component, char *file_path);
#endif
