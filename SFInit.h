/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Initialisation
 *  Copyright (C) 2020  Chris Bazley
 */

#ifndef SFInit_h
#define SFInit_h

#include "toolbox.h"
#include "Macros.h"
#include "PalEntry.h"
#include "SprFormats.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

#define APP_NAME "SFEditor"

enum
{
  NumColours = 256
};

extern PaletteEntry const (*palette)[NumColours];
extern char taskname[];
extern int wimp_version, task_handle;
extern MessagesFD messages;
extern _Optional SpriteAreaHeader *tb_sprite_area;

void initialise(void);

#endif
