/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal graphics set
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef ObjGfx_h
#define ObjGfx_h

#include <stdbool.h>
#include "Dfile.h"

typedef struct ObjGfx ObjGfx;

void ObjGfx_init(void);
ObjGfx *ObjGfx_get_shared(char const *filename);
ObjGfx *ObjGfx_create(void);
bool ObjGfx_share(ObjGfx *gfx);
DFile *ObjGfx_get_dfile(ObjGfx *gfx);
void ObjGfx_load_metadata(ObjGfx *gfx);

#endif
