/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Veneers to the operating system's graphics facilities
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef Plot_h
#define Plot_h

#include <stdbool.h>
#include "PalEntry.h"
#include "Vertex.h"

#define PAL_WHITE (PaletteEntry_RedMask | \
                   PaletteEntry_GreenMask | \
                   PaletteEntry_BlueMask)

#define PAL_BLACK (0)

void plot_set_window(BBox const *bbox);
void plot_get_window(BBox *bbox);

void plot_set_wimp_col(int colour);
void plot_set_col(PaletteEntry colour);
void plot_set_bg_col(PaletteEntry colour);
void plot_set_native_col(int colour);

void plot_clear_window(void);
bool plot_can_blend_font(void);
bool plot_find_font(Vertex size, int *handle);

typedef struct {
  Vertex space_offset;
  Vertex letter_offset;
  BBox rubout;
} Plot_FontParams;

void plot_font(int handle, char const *string,
  BBox const *rubout, Vertex scr_pos, bool blend);

int plot_get_font_width(int handle, char const *string);

void plot_get_char_bbox(int handle, BBox *bbox);

void plot_get_string_bbox(int handle, char const *string,
  BBox *bbox);

void plot_set_font_col(int handle,
  PaletteEntry bg_colour, PaletteEntry fg_colour);

void plot_lose_font(int handle);
void plot_move(Vertex scr_pos);
void plot_fg_point(Vertex scr_pos);
void plot_fg_line(Vertex scr_pos);
void plot_fg_line_ex_start(Vertex scr_pos);
void plot_fg_line_ex_end(Vertex scr_pos);
void plot_fg_line_ex_both(Vertex scr_pos);
void plot_fg_dot_line(Vertex scr_pos);
void plot_set_dot_pattern_len(int len);
void plot_set_dot_pattern(unsigned char (*bitmap)[8]);
void plot_set_dash_pattern(int len);

void plot_fg_bbox(BBox const *bbox);
void plot_inv_bbox(BBox const *bbox);

void plot_fg_tri(Vertex scr_pos);
void plot_fg_rect_2v(Vertex scr_pos_1, Vertex scr_pos_2);
void plot_inv_dot_rect_2v(Vertex scr_pos_1, Vertex scr_pos_2);
void plot_fg_ol_rect_2v(Vertex scr_pos_1, Vertex scr_pos_2);
void plot_fg_dot_rect_2v(Vertex scr_pos_1, Vertex scr_pos_2);

#endif
