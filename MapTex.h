/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map textures set
 *  Copyright (C) 2007 Christopher Bazley
 */

#ifndef MapTex_h
#define MapTex_h

#include <stdbool.h>
#include "Dfile.h"

typedef struct MapTex MapTex;

void MapTex_init(void);
MapTex *MapTex_get_shared(char const *filename);
MapTex *MapTex_create(void);
bool MapTex_share(MapTex *textures);
DFile *MapTex_get_dfile(MapTex *textures);
void MapTex_load_metadata(MapTex *textures);

#endif
