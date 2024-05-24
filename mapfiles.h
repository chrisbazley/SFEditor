/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  File menu (map version)
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef mapfiles_h
#define mapfiles_h

#include "toolbox.h"

#include "Session.h"
#include "DataType.h"

extern ObjectId MapFiles_sharedid;

void MapFiles_created(ObjectId const id);

DataType MapFiles_get_data_type(ComponentId menu_entry);

#endif
