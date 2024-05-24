/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot area of the ground map to a specified sprite
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef DrawTiles_h
#define DrawTiles_h

#include <stdbool.h>
#include "Vertex.h"
#include "MapCoord.h"
#include "SFInit.h"
#include "Map.h"

enum {
  /* The eigen values must be correct for the screen mode number.
     They affect the size of thumbnails in the palette but that's all. */
  DrawTilesModeNumber = 28, /* small thumbnails */
  DrawTilesModeXEig = 1,
  DrawTilesModeYEig = 1,
  DrawTilesModeLog2BPP = 3,
};

typedef struct {
  MapRef tile_ref;
  bool is_selected;
} DrawTilesReadResult;

typedef DrawTilesReadResult DrawTilesReadFn(void *cb_arg, MapPoint map_pos);

struct MapTexBitmaps;
struct SprMem;
struct MapEditSelection;

bool DrawTiles_to_sprite(struct MapTexBitmaps *tilesdata,
  struct SprMem *sm, char const *name, MapAngle angle, MapArea const *scr_area,
  DrawTilesReadFn *read, void *cb_arg, int zoom,
  unsigned char const (*sel_colours)[NumColours]);

typedef void DrawTilesBBoxFn(void *cb_arg, BBox const *bbox, MapRef value);

void DrawTiles_to_mask(struct SprMem *sm, char const *name,
  MapAngle angle, MapArea const *scr_area, DrawTilesReadFn *read, void *cb_arg,
  int zoom);

void DrawTiles_to_bbox(
  MapAngle angle, MapArea const *scr_area, DrawTilesReadFn *read, void *read_arg,
  DrawTilesBBoxFn *give_bbox, void *give_bbox_arg, Vertex tile_size);

#endif
