/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Initialisation
 *  Copyright (C) 2020  Chris Bazley
 */

#ifndef SFInit_h
#define SFInit_h

#include "toolbox.h"

#include "PalEntry.h"

#define APP_NAME "SFEditor"

enum
{
  NumColours = 256
};

extern PaletteEntry const (*palette)[NumColours];
extern char taskname[];
extern int wimp_version, task_handle;
extern MessagesFD messages;
extern void *tb_sprite_area;

void initialise(void);

#endif
