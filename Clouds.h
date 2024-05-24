/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Cloud colours
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Clouds_h
#define Clouds_h

#include "SFError.h"

struct Reader;
struct Writer;

enum {
  Clouds_NumColours = 2
};

typedef struct CloudColData CloudColData;

SFError clouds_read(CloudColData *clouds, struct Reader *reader);

void clouds_write(CloudColData const *clouds, struct Writer *writer);

unsigned int clouds_get_colour(CloudColData const *clouds, size_t index);
void clouds_set_colour(CloudColData *clouds, size_t index, unsigned int colour);

#endif
