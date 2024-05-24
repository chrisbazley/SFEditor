/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygon colours
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef PolyCol_h
#define PolyCol_h

#include <stdbool.h>
#include "DFile.h"

enum {
  PolyColMax = 320,
};

typedef struct PolyColData PolyColData;

void polycol_init(void);
PolyColData *polycol_get_shared(char const *filename);
PolyColData *polycol_create(void);
bool polycol_share(PolyColData *poly_colours);
DFile *polycol_get_dfile(PolyColData *poly_colours);
size_t polycol_get_colour(PolyColData const *poly_colours, size_t index);

#endif
