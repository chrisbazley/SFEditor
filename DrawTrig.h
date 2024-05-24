/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Draw mission action triggers
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef DrawTrig_h
#define DrawTrig_h

#include <stdbool.h>

#include "PalEntry.h"
#include "Obj.h"
#include "Triggers.h"
#include "Vertex.h"
#include "SprFormats.h"

enum {
  DrawTrigNumColours = 2
};

typedef enum {
  DrawTrigSel_No,
  DrawTrigSel_Yes,
  DrawTrigSel_Count
} DrawTrigSel;

typedef struct {
  unsigned char trans_table[DrawTrigSel_Count][DrawTrigNumColours];
  int zoom;
} DrawTrigContext;

Vertex DrawTrig_get_max_size_os(void);

bool DrawTrig_init(DrawTrigContext *context,
  PaletteEntry (*colours)[DrawTrigNumColours],
  PaletteEntry (*sel_colours)[DrawTrigNumColours],
  int zoom);

void DrawTrig_plot(DrawTrigContext const *context,
  TriggerParam param, Vertex scr_pos, bool is_selected);

void DrawTrig_plot_defence(DrawTrigContext const *context,
  ObjRef obj_ref, Vertex scr_pos, bool is_selected);

#endif
