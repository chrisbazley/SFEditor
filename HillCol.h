/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Hill colours
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef HillCol_h
#define HillCol_h

#include <stdbool.h>
#include "DFile.h"

enum {
  HillNumColours = 36,
};

typedef struct HillColData HillColData;

void hillcol_init(void);
HillColData *hillcol_create(void);
HillColData *hillcol_get_shared(char const *filename);
bool hillcol_share(HillColData *hill_colours);
DFile *hillcol_get_dfile(HillColData *hill_colours);
size_t hillcol_get_colour(HillColData const *hill_colours, int index);

#endif
