/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map editing view
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef View_h
#define View_h

#include <stdbool.h>

#include "ObjGfxMesh.h"

enum {
  TexelToOSCoordLog2 = 1,
  EditWinZoomMax = 4,
  EditWinZoomDefault = 3,
  EditWinZoomMin = -2,
};

/* Don't add anything that isn't displayed in the work area
   because a full redraw is triggered whenever one of these
   flags changes */
typedef struct {
  bool MAP:1, MAP_OVERLAY:1, MAP_ANIMS:1,
       OBJECTS:1, OBJECTS_OVERLAY:1,
       SHIPS:1, GRID:1, NUMBERS:1, INFO:1;
} ViewDisplayFlags;

/* The downside of storing display flags in a struct instead of an
   integer is that we need a comparison function: */
static inline bool ViewDisplayFlags_equal(ViewDisplayFlags const a,
  ViewDisplayFlags const b)
{
  return a.MAP == b.MAP &&
         a.MAP_OVERLAY == b.MAP_OVERLAY &&
         a.MAP_ANIMS == b.MAP_ANIMS &&
         a.OBJECTS == b.OBJECTS &&
         a.OBJECTS_OVERLAY == b.OBJECTS_OVERLAY &&
         a.SHIPS == b.SHIPS &&
         a.GRID == b.GRID &&
         a.NUMBERS == b.NUMBERS &&
         a.INFO == b.INFO;
}

typedef struct ViewConfig {
  bool show_status_bar;
  ViewDisplayFlags flags;
  int zoom_factor; /* plotwidth = texwidth >> zoom
                      -2:32×, -1:16×, 0:8×, 1:4×, 2:2×, 3:1×, 4:½×
                      (the 'magnification' levels are fairly arbitrary,
                      copied from the mission map within the game */
  MapAngle angle;
  PaletteEntry grid_colour, back_colour, ghost_colour, sel_colour;
} ViewConfig;

typedef struct View {
  ViewConfig config;
  ObjGfxMeshesView plot_ctx;
  unsigned char map_units_per_os_unit_log2; /* from the zoom factor */
  Vertex map_size_in_os_units; /* from the zoom factor */
  unsigned char sel_colours[NumColours];
  PaletteEntry sel_palette[NumColours];
} View;

#endif
