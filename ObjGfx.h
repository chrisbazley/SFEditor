/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal graphics set
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef ObjGfx_h
#define ObjGfx_h

#include <stdbool.h>
#include "DFile.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

typedef struct ObjGfx ObjGfx;

void ObjGfx_init(void);
_Optional ObjGfx *ObjGfx_get_shared(char const *filename);
_Optional ObjGfx *ObjGfx_create(void);
bool ObjGfx_share(ObjGfx *gfx);
DFile *ObjGfx_get_dfile(ObjGfx *gfx);
void ObjGfx_load_metadata(ObjGfx *gfx);

#endif
