/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Pre-quit dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef PreQuit_h
#define PreQuit_h

#include <stdbool.h>
#include "toolbox.h"

void PreQuit_created(ObjectId object);
bool PreQuit_queryunsaved(int task_handle);

#endif
