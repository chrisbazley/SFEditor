/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Veneers to the operating system's graphics facilities
 *  Copyright (C) 2001 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <limits.h>

#include "wimplib.h"
#include "kernel.h"
#include "swis.h"

#include "wimpextra.h"
#include "OSVDU.h"
#include "ClrTrans.h"
#include "Macros.h"
#include "err.h"
#include "debug.h"

#include "plot.h"

enum {
  MILLIPOINTS_PER_UNIT = 400,
  ClearGraphicsWindow = 16,
  SetGraphicsWindow = 24,
  RWGraphicsInfoR0 = 163,
  RWGraphicsInfoR1 = 242,
  DefaultPattern = 0,
  MiscCommand = 23,
  MiscSetDotPattern = 6,
};

static int millipoints_to_os(int const mp)
{
  return (mp + MILLIPOINTS_PER_UNIT - 1) / MILLIPOINTS_PER_UNIT;
}

void plot_set_dot_pattern_len(int const len)
{
  _kernel_swi_regs regs = {{RWGraphicsInfoR0, RWGraphicsInfoR1, len}};
  DEBUGF("Setting dot pattern length %d\n", len);
  E(_kernel_swi(OS_Byte, &regs, &regs));
}

void plot_set_dot_pattern(unsigned char (*const bitmap)[8])
{
  assert(bitmap);
  _kernel_oswrch(MiscCommand);
  _kernel_oswrch(MiscSetDotPattern);
  for (size_t i = 0; i < ARRAY_SIZE(*bitmap); ++i) {
    DEBUGF("Setting dot pattern byte %zu: 0x%x\n", i, (*bitmap)[i]);
    _kernel_oswrch((*bitmap)[i]);
  }
}

void plot_set_dash_pattern(int const len)
{
  unsigned char bitmap[8] = {0};
  int plen = 2 * len;
  plen = HIGHEST(CHAR_BIT, plen);
  plen = LOWEST(CHAR_BIT * (int)ARRAY_SIZE(bitmap), plen);
  plot_set_dot_pattern_len(plen);

  for (int i = 0; i < plen; ++i) {
    int const dash_num = len ? i / len : 0;
    if (dash_num % 2) {
      bitmap[i / CHAR_BIT] |= 1 << (i % CHAR_BIT);
    }
  }
  plot_set_dot_pattern(&bitmap);
}

void plot_set_window(BBox const *const bbox)
{
  assert(bbox);
  DEBUGF("Setting graphics window to %d,%d,%d,%d\n",
    bbox->xmin, bbox->ymin, bbox->xmax, bbox->ymax);

  _kernel_oswrch(SetGraphicsWindow);
  _kernel_oswrch(bbox->xmin & UINT8_MAX);
  _kernel_oswrch((bbox->xmin >> 8) & UINT8_MAX);
  _kernel_oswrch(bbox->ymin & UINT8_MAX);
  _kernel_oswrch((bbox->ymin >> 8) & UINT8_MAX);
  _kernel_oswrch(bbox->xmax & UINT8_MAX);
  _kernel_oswrch((bbox->xmax >> 8) & UINT8_MAX);
  _kernel_oswrch(bbox->ymax & UINT8_MAX);
  _kernel_oswrch((bbox->ymax >> 8) & UINT8_MAX);
}

void plot_get_window(BBox *const bbox)
{
  assert(bbox);

  VDUVar var_ids[] = {VDUVar_GWLCol, VDUVar_GWBRow, VDUVar_GWRCol, VDUVar_GWTRow,
    ModeVar_XEigFactor,
    ModeVar_YEigFactor,
    VDUVar_OrgX,
    VDUVar_OrgY,
    VDUVar_EndOfList};
  int values[ARRAY_SIZE(var_ids)];
  E(os_read_vdu_variables(var_ids, values));

  bbox->xmin = values[6] + (values[0] << values[4]);
  bbox->ymin = values[7] + (values[1] << values[5]);
  bbox->xmax = values[6] + (values[2] << values[4]);
  bbox->ymax = values[7] +  (values[3] << values[5]);

  DEBUGF("Got graphics window %d,%d,%d,%d\n",
    bbox->xmin, bbox->ymin, bbox->xmax, bbox->ymax);
}

void plot_set_wimp_col(int colour)
{
  assert(colour >= WimpColour_White);
  assert(colour <= WimpColour_LightBlue);
  DEBUG_VERBOSEF("Setting wimp colour %d\n", colour);
  E(wimp_set_colour(colour));
}

void plot_set_col(PaletteEntry const colour)
{
  DEBUG_VERBOSEF("Setting 24-bit plot colour 0x%x\n", colour);
  E(colourtrans_set_gcol(
    ColourTrans_SetGCOL_UseECF, GCOLAction_Overwrite, colour));
}

void plot_set_bg_col(PaletteEntry const colour)
{
  DEBUG_VERBOSEF("Setting 24-bit background colour 0x%x\n", colour);
  E(colourtrans_set_gcol(
    ColourTrans_SetGCOL_Background|ColourTrans_SetGCOL_UseECF,
    GCOLAction_Overwrite, colour));
}

void plot_set_native_col(int colour)
{
  DEBUG_VERBOSEF("Setting plot colour 0x%x\n", colour);
  E(os_set_colour(0, GCOLAction_Overwrite, colour));
}

void plot_clear_window(void)
{
  _kernel_oswrch(ClearGraphicsWindow);
}

bool plot_can_blend_font(void)
{
  int version;
  if (E(_swix(Font_CacheAddr, _OUT(0), &version))) {
    version = 0;
  }
  bool const can_blend = (version >= 335);
  DEBUG_VERBOSEF("%s blend font with background\n",
                 can_blend ? "Can" : "Cannot");
  return can_blend;
}

bool plot_find_font(Vertex const size, int *const handle)
{
  assert(handle != NULL);
  bool success = false;
  if (!E(_swix(Font_FindFont, _INR(1,5)|_OUT(0), "Corpus.Bold",
               size.x * 16, size.y * 16, 0, 0, handle))) {
    DEBUG_VERBOSEF("Found font handle %d for size %d,%d\n",
      *handle, size.x, size.y);
    success = true;
  } else {
    DEBUGF("Failed to get font handle for size %d,%d\n",
      size.x, size.y);
  }
  return success;
}

int plot_get_font_width(int const handle, char const *const string)
{
  assert(string != NULL);
  int width;
  if (E(_swix(Font_ScanString, _INR(0,4)|_OUT(3), handle, string,
              (1<<8), INT_MAX, INT_MAX, &width))) {
    width = 0;
  } else {
    width = millipoints_to_os(width);
  }
  DEBUG_VERBOSEF("Width of '%s' with font handle %d is %d\n",
    string, handle, width);
  return width;
}


void plot_get_char_bbox(int const handle, BBox *const bbox)
{
  assert(bbox != NULL);
  if (E(_swix(Font_ReadInfo, _IN(0)|_OUTR(1,4), handle,
              &bbox->xmin, &bbox->ymin, &bbox->xmax, &bbox->ymax))) {
    *bbox = (BBox){0,0,0,0};
  }
}

void plot_get_string_bbox(int const handle, char const *const string,
  BBox *const bbox)
{
  assert(string != NULL);
  assert(bbox != NULL);
  int justify_and_rubout[9] = {0,0,0,0,-1}; /* no split character */

  if (E(_swix(Font_ScanString, _INR(0,5), handle, string,
              (1<<8) | (1<<5) | (1<<18),
              INT_MAX, INT_MAX, justify_and_rubout))) {
    *bbox = (BBox){0,0,0,0};
  } else {
    bbox->xmin = millipoints_to_os(justify_and_rubout[5]);
    bbox->ymin = millipoints_to_os(justify_and_rubout[6]);
    bbox->xmax = millipoints_to_os(justify_and_rubout[7]);
    bbox->ymax = millipoints_to_os(justify_and_rubout[8]);
  }
  DEBUG("Bounding box of '%s' with font handle %d is %d,%d %d,%d",
        string, handle, bbox->xmin, bbox->ymin, bbox->xmax, bbox->ymax);
}

void plot_font(int const handle, char const *const string,
  BBox const *const rubout, Vertex const scr_pos, bool blend)
{
  assert(string != NULL);
  DEBUG_VERBOSEF("Plotting font handle %d string '%s' at %d,%d (%s blending)\n",
        handle, string, scr_pos.x, scr_pos.y, blend ? "with" : "without");

  int paint_flags = 1<<4; /* coords specified in OS units */;

  if (rubout) {
    plot_move((Vertex){rubout->xmin, rubout->ymin});
    plot_move((Vertex){rubout->xmax, rubout->ymax});
    SET_BITS(paint_flags, (1<<1));
  }

  if (blend) {
    SET_BITS(paint_flags, 1<<11); /* enable background blending */
  }

  E(_swix(Font_Paint, _INR(0,4),
    handle, string, paint_flags, scr_pos.x, scr_pos.y));
}

void plot_set_font_col(int const handle, PaletteEntry const bg_colour,
  PaletteEntry const fg_colour)
{
  DEBUG_VERBOSEF("Setting 24-bit colours 0x%x, 0x%x for font handle %d\n",
    bg_colour, fg_colour, handle);

  E(_swix(ColourTrans_SetFontColours, _INR(0,3),
                   handle, bg_colour, fg_colour, 14));
}

void plot_lose_font(int const handle)
{
  DEBUG_VERBOSEF("Losing font handle %d", handle);
  E(_swix(Font_LoseFont, _IN(0), handle));
}

void plot_move(Vertex const scr_pos)
{
  DEBUG_VERBOSEF("Moving graphics cursor to %d,%d\n", scr_pos.x, scr_pos.y);
  E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                     scr_pos.x, scr_pos.y));
}

void plot_fg_point(Vertex const scr_pos)
{
  E(os_plot(PlotOp_Point + PlotOp_PlotFGAbs, scr_pos.x, scr_pos.y));
}

void plot_fg_line(Vertex const scr_pos)
{
  DEBUG_VERBOSEF("Plotting foreground line to %d,%d\n",
    scr_pos.x, scr_pos.y);
  E(os_plot(PlotOp_SolidInclBoth + PlotOp_PlotFGAbs,
                     scr_pos.x, scr_pos.y));
}

void plot_fg_line_ex_start(Vertex const scr_pos)
{
  DEBUG_VERBOSEF("Plotting foreground line (ex. start) to %d,%d\n",
    scr_pos.x, scr_pos.y);
  E(os_plot(PlotOp_SolidExclStart + PlotOp_PlotFGAbs,
                     scr_pos.x, scr_pos.y));
}

void plot_fg_line_ex_end(Vertex const scr_pos)
{
  DEBUG_VERBOSEF("Plotting foreground line (ex. end) to %d,%d\n",
    scr_pos.x, scr_pos.y);
  E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGAbs,
                     scr_pos.x, scr_pos.y));
}

void plot_fg_line_ex_both(Vertex const scr_pos)
{
  DEBUG_VERBOSEF("Plotting foreground line (ex. both) to %d,%d\n",
    scr_pos.x, scr_pos.y);
  E(os_plot(PlotOp_SolidExclBoth + PlotOp_PlotFGAbs,
                     scr_pos.x, scr_pos.y));
}

void plot_fg_bbox(BBox const *const bbox)
{
  assert(BBox_is_valid(bbox));
  DEBUG_VERBOSEF("Plotting foreground bounding box from %d,%d to %d,%d\n",
    bbox->xmin, bbox->ymin, bbox->xmax, bbox->ymax);

  E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                     bbox->xmin, bbox->ymin));

  E(os_plot(PlotOp_RectangleFill + PlotOp_PlotFGAbs,
                     bbox->xmax - 1, bbox->ymax - 1));
}

void plot_inv_bbox(BBox const *const bbox)
{
  assert(BBox_is_valid(bbox));
  DEBUG_VERBOSEF("Plotting inverted bounding box from %d,%d to %d,%d\n",
    bbox->xmin, bbox->ymin, bbox->xmax, bbox->ymax);

  E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                     bbox->xmin, bbox->ymin));

  E(os_plot(PlotOp_RectangleFill + PlotOp_PlotInvAbs,
                     bbox->xmax - 1, bbox->ymax - 1));
}

void plot_fg_dot_line(Vertex const scr_pos)
{
  DEBUG_VERBOSEF("Plotting foreground dotted line to %d,%d\n",
    scr_pos.x, scr_pos.y);

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotFGAbs,
                     scr_pos.x, scr_pos.y));
}

void plot_fg_rect_2v(Vertex const scr_pos_1, Vertex const scr_pos_2)
{
  DEBUG_VERBOSEF("Plotting foreground rectangle from %d,%d to %d,%d\n",
    scr_pos_1.x, scr_pos_1.y, scr_pos_2.x, scr_pos_2.y);

  E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                     scr_pos_1.x, scr_pos_1.y));

  E(os_plot(PlotOp_RectangleFill + PlotOp_PlotFGAbs,
                     scr_pos_2.x, scr_pos_2.y));
}

void plot_inv_dot_rect_2v(Vertex const scr_pos_1, Vertex const scr_pos_2)
{
  DEBUG_VERBOSEF("Plotting inverted dotted rectangle from %d,%d to %d,%d\n",
    scr_pos_1.x, scr_pos_1.y, scr_pos_2.x, scr_pos_2.y);

  E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                     scr_pos_1.x, scr_pos_1.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotInvAbs,
                     scr_pos_1.x, scr_pos_2.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotInvAbs,
                     scr_pos_2.x, scr_pos_2.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotInvAbs,
                     scr_pos_2.x, scr_pos_1.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotInvAbs,
                     scr_pos_1.x, scr_pos_1.y));
}

void plot_fg_ol_rect_2v(Vertex const scr_pos_1, Vertex const scr_pos_2)
{
  DEBUG_VERBOSEF("Plotting outline rectangle from %d,%d to %d,%d\n",
    scr_pos_1.x, scr_pos_1.y, scr_pos_2.x, scr_pos_2.y);

  E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                     scr_pos_1.x, scr_pos_1.y));

  E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGAbs,
                     scr_pos_1.x, scr_pos_2.y));

  E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGAbs,
                     scr_pos_2.x, scr_pos_2.y));

  E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGAbs,
                     scr_pos_2.x, scr_pos_1.y));

  E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGAbs,
                     scr_pos_1.x, scr_pos_1.y));
}

void plot_fg_dot_rect_2v(Vertex const scr_pos_1, Vertex const scr_pos_2)
{
  DEBUG_VERBOSEF("Plotting dotted rectangle from %d,%d to %d,%d\n",
    scr_pos_1.x, scr_pos_1.y, scr_pos_2.x, scr_pos_2.y);

  E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                     scr_pos_1.x, scr_pos_1.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotFGAbs,
                     scr_pos_1.x, scr_pos_2.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotFGAbs,
                     scr_pos_2.x, scr_pos_2.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotFGAbs,
                     scr_pos_2.x, scr_pos_1.y));

  E(os_plot(PlotOp_StartDottedInclBoth + PlotOp_PlotFGAbs,
                     scr_pos_1.x, scr_pos_1.y));
}

void plot_fg_tri(Vertex const scr_pos)
{
  DEBUG_VERBOSEF("Plotting foreground triangle to %d,%d\n",
    scr_pos.x, scr_pos.y);

  E(os_plot(PlotOp_PlotFGAbs + PlotOp_TriangleFill,
                     scr_pos.x, scr_pos.y));
}
