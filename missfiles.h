/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  File menu (mission version)
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef missfiles_h
#define missfiles_h

#include "toolbox.h"

#include "Session.h"
#include "DataType.h"

extern ObjectId MissFiles_sharedid;

void MissFiles_created(ObjectId const id);

DataType MissFiles_get_data_type(ComponentId menu_entry);

#endif
