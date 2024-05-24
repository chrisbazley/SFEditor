/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Tiles palette menu
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef TilesMenu_h
#define TilesMenu_h

#include "toolbox.h"
#include "Palette.h"

void TilesMenu_created(ObjectId id);
void TilesMenu_attach(PaletteData *pal_data);
void TilesMenu_update(PaletteData *pal_data);

#endif
