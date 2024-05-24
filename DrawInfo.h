/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Draw strategic target information
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef DrawInfo_h
#define DrawInfo_h

#include <stdbool.h>

#include "SFInit.h"
#include "PalEntry.h"
#include "Vertex.h"
#include "SprFormats.h"
#include "Infos.h"

enum {
  DrawInfoPaletteSize = 2,
};

typedef enum {
  DrawInfoSel_No,
  DrawInfoSel_Yes,
  DrawInfoSel_Count
} DrawInfoSel;

typedef struct {
  unsigned char trans_table[DrawInfoSel_Count][TargetInfoMax][DrawInfoPaletteSize];
  ScaleFactors scale_factors;
  Vertex plot_offset;
  char const *sprite_name;
} DrawInfoContext;

Vertex DrawInfo_get_size_os(bool is_ghost);

bool DrawInfo_init(DrawInfoContext *context,
  PaletteEntry (*colours)[TargetInfoMax][DrawInfoPaletteSize],
  PaletteEntry (*sel_colours)[TargetInfoMax][DrawInfoPaletteSize],
  int zoom, bool is_ghost);

void DrawInfo_plot(DrawInfoContext const *context, Vertex scr_pos,
  bool is_selected, int id);

#endif
