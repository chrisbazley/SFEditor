/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Draw clouds
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef DrawCloud_h
#define DrawCloud_h

#include <stdbool.h>

#include "SFInit.h"
#include "PalEntry.h"
#include "Clouds.h"
#include "Vertex.h"
#include "SprFormats.h"

typedef enum {
  DrawCloudSel_No,
  DrawCloudSel_Yes,
  DrawCloudSel_Count
} DrawCloudSel;

typedef struct {
  unsigned char trans_table[DrawCloudSel_Count][Obj_NumCloudTints][Clouds_NumColours];
  ScaleFactors scale_factors;
  bool is_ghost;
} DrawCloudContext;

Vertex DrawCloud_get_size_os(void);

bool DrawCloud_init(DrawCloudContext *context, CloudColData const *clouds,
  PaletteEntry const (*palette)[NumColours], PaletteEntry const (*sel_palette)[NumColours],
  int zoom, bool is_ghost);

void DrawCloud_plot(DrawCloudContext const *context, Vertex scr_pos,
  bool is_selected, int cloud_type);

#endif
