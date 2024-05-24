/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map transfers palette menu
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef TransMenu_h
#define TransMenu_h

#include "toolbox.h"
#include "Palette.h"

void TransMenu_created(ObjectId const id);
void TransMenu_attach(PaletteData *pal_data);
void TransMenu_update(PaletteData *pal_data);

#endif
