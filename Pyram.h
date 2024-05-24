/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission position in pyramid
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Pyram_h
#define Pyram_h

struct Reader;
struct Writer;

#include "SFError.h"

typedef enum
{
  Pyramid_Easy,
  Pyramid_Medium,
  Pyramid_Hard,
  Pyramid_User,
}
Pyramid;

typedef struct PyramidData PyramidData;

SFError pyramid_read(PyramidData *pyramid, struct Reader *reader);
void pyramid_write(PyramidData const *pyramid, struct Writer *writer);

void pyramid_set_position(PyramidData *pyramid,
  Pyramid difficulty, int level_number);

Pyramid pyramid_get_difficulty(PyramidData const *pyramid);
int pyramid_get_level_number(PyramidData const *pyramid);

#endif
