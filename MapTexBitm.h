/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map texture bitmaps
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef MapTexBitm_h
#define MapTexBitm_h

struct Reader;
#include "SFError.h"
#include "SprMem.h"
#include "MapCoord.h"
#include "Map.h"

enum {
  MapTexSizeLog2 = 4, /* 16 pixels */
  MapTexSize = 1 << MapTexSizeLog2, /* in pixels */

  /* The eigen values must be correct for the screen mode number.
     They affect the size of thumbnails in the palette but that's all. */
  MapTexModeNumber = 13, /* big thumbnails */
  MapTexModeXEig = 2,
  MapTexModeYEig = 2,
  MapTexMax = 192, // game only allocates space for 192 bitmaps
};

typedef struct MapTexBitmaps MapTexBitmaps;

void MapTexBitmaps_init(MapTexBitmaps *tiles);
size_t MapTexBitmaps_get_count(const MapTexBitmaps *tiles);
SFError MapTexBitmaps_read(MapTexBitmaps *tiles, struct Reader *reader);
void MapTexBitmaps_free(MapTexBitmaps *tiles);
bool MapTexBitmaps_is_bright(const MapTexBitmaps *tiles, MapRef tile_num);
int MapTexBitmaps_get_average_colour(const MapTexBitmaps *tiles, MapRef tile_num);
SprMem *MapTexBitmaps_get_sprites(MapTexBitmaps *tiles, MapAngle angle, int level);
#endif
