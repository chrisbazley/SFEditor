/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot a specified area of the ground map
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

#include "stdlib.h"
#include <assert.h>
#include <limits.h>

#include "wimp.h"

#include "SprFormats.h"

#include "fastplot.h"

#ifdef FASTPLOT
/* Eventually we want to replace these routines with hand-optimised ARM code */

bool FastPlot_plotarea(SpriteAreaHeader *tile_sprites, SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns])
{
  /*  1:1 (16x16) */
  bool needs_mask = false;
  size_t bytes_across_buf = (buffer->width + 1) * 4;
  unsigned char *first_teximage = (unsigned char *)tile_sprites +
                                  tile_sprites->first + sizeof(SpriteHeader);
  /* (assumes that first sprite has no palette) */

  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->image +
                                  (buffer->width + 1) * 4 *
                                  ((buffer->height + 1) - SFMapTile_Height);

  assert(base != NULL || overlay != NULL);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      /* Read tile number from overlay or base map? */
      char tile_num;

      if (overlay == NULL) {
        if (base != NULL)
          tile_num = base[row][col]; /* we have no overlay - read tile from basemap */
        else
          tile_num = UINT8_MAX; /* no base map either */
      } else {
        tile_num = overlay[row][col]; /* read tile from map overlay */
        if (base != NULL && tile_num == UINT8_MAX) {
          /* we have both basemap and overlay, overlay is transparent */
          tile_num = base[row][col]; /* read tile from basemap */
        }
      }

      {
        /* Plot tile */
        unsigned char *line_plot_addr = tile_plot_addr;
        unsigned long *tile_read_addr = NULL;
        if (tile_num != UINT8_MAX) {
          if (tile_num >= tile_sprites->sprite_count)
            tile_num = 0; /* FIXME: substitute a placeholder sprite? */

          tile_read_addr = (unsigned long *)(first_teximage + tile_num *
                           (sizeof(SpriteHeader) + sizeof(SFMapTile)));
          /* (assumes sprites stored consecutively and have no palette/mask) */
        } else
          needs_mask = true;

        if (inv_map != NULL && inv_map[row][col] != 0) {
          if (tile_num != UINT8_MAX) {
            for (int line = 0; line < SFMapTile_Height; line++) {
              ((unsigned long *)line_plot_addr)[0] = *tile_read_addr++ ^ ULONG_MAX;
              ((unsigned long *)line_plot_addr)[1] = *tile_read_addr++ ^ ULONG_MAX;
              ((unsigned long *)line_plot_addr)[2] = *tile_read_addr++ ^ ULONG_MAX;
              ((unsigned long *)line_plot_addr)[3] = *tile_read_addr++ ^ ULONG_MAX;

              line_plot_addr += bytes_across_buf;
            }
          }
        } else {
          if (tile_num != UINT8_MAX) {
            for (int line = 0; line < SFMapTile_Height; line++) {
              ((unsigned long *)line_plot_addr)[0] = *tile_read_addr++;
              ((unsigned long *)line_plot_addr)[1] = *tile_read_addr++;
              ((unsigned long *)line_plot_addr)[2] = *tile_read_addr++;
              ((unsigned long *)line_plot_addr)[3] = *tile_read_addr++;

              line_plot_addr += bytes_across_buf;
            }
          }
        }
      }
      tile_plot_addr += SFMapTile_Width;
    }
    plot_row_start -= bytes_across_buf * SFMapTile_Height;
  }
  return needs_mask;
}

bool FastPlot_plotareaB(SpriteAreaHeader *tile_sprites, SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns])
{
  /*  1:2 (8x8) */
  bool needs_mask = false;
  size_t bytes_across_buf = (buffer->width+1) * 4;
  unsigned char *first_teximage = (unsigned char *)tile_sprites +
                                  tile_sprites->first + sizeof(SpriteHeader);
  /* (assumes that first sprite has no palette) */

  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->image +
                                  bytes_across_buf * ((buffer->height + 1) -
                                  SFMapTile_Height / 2);

  assert(base != NULL || overlay != NULL);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      /* Read tile number from overlay or base map? */
      char tile_num;

      if (overlay == NULL) {
        if (base != NULL)
          tile_num = base[row][col]; /* we have no overlay - read tile from basemap */
        else
          tile_num = UINT8_MAX; /* no base map either */
      } else {
        tile_num = overlay[row][col]; /* read tile from map overlay */
        if (base != NULL && tile_num == UINT8_MAX) {
          /* we have both basemap and overlay, overlay is transparent */
          tile_num = base[row][col]; /* read tile from basemap */
        }
      }

      {
        /* Plot tile */
        unsigned char *tile_read_addr = NULL, *line_plot_addr = tile_plot_addr;
        if (tile_num != UINT8_MAX) {
          if (tile_num >= tile_sprites->sprite_count)
            tile_num = 0; /* FIXME: substitute a placeholder sprite? */

          tile_read_addr = first_teximage + tile_num * (sizeof(SpriteHeader) +
                           sizeof(SFMapTile));
          /* (assumes sprites stored consecutively and have no palette/mask) */
        } else
          needs_mask = true;


        if (inv_map != NULL && inv_map[row][col] != 0) {
          if (tile_num != UINT8_MAX) {
            for (int line = 0; line < (SFMapTile_Height/2); line++) {
              line_plot_addr[0] = tile_read_addr[0] ^ UCHAR_MAX;
              line_plot_addr[1] = tile_read_addr[2] ^ UCHAR_MAX;
              line_plot_addr[2] = tile_read_addr[4] ^ UCHAR_MAX;
              line_plot_addr[3] = tile_read_addr[6] ^ UCHAR_MAX;
              line_plot_addr[4] = tile_read_addr[8] ^ UCHAR_MAX;
              line_plot_addr[5] = tile_read_addr[10] ^ UCHAR_MAX;
              line_plot_addr[6] = tile_read_addr[12] ^ UCHAR_MAX;
              line_plot_addr[7] = tile_read_addr[14] ^ UCHAR_MAX;

              tile_read_addr += SFMapTile_Width*2;
              line_plot_addr += bytes_across_buf;
            }
          }
        } else {
          if (tile_num != UINT8_MAX) {
            for (int line = 0; line < SFMapTile_Height/2; line++) {
              line_plot_addr[0] = tile_read_addr[0];
              line_plot_addr[1] = tile_read_addr[2];
              line_plot_addr[2] = tile_read_addr[4];
              line_plot_addr[3] = tile_read_addr[6];
              line_plot_addr[4] = tile_read_addr[8];
              line_plot_addr[5] = tile_read_addr[10];
              line_plot_addr[6] = tile_read_addr[12];
              line_plot_addr[7] = tile_read_addr[14];

              tile_read_addr += SFMapTile_Width*2;
              line_plot_addr += bytes_across_buf;
            }
          }
        }
      }
      tile_plot_addr += SFMapTile_Width/2;
    }
    plot_row_start -= bytes_across_buf * (SFMapTile_Height/2);
  }
  return needs_mask;
}

bool FastPlot_plotareaC(SpriteAreaHeader *tile_sprites, SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns])
{
  /*  1:4 (4x4) */
  bool needs_mask = false;
  size_t bytes_across_buf = (buffer->width+1) * 4;
  unsigned char *first_teximage = (unsigned char *)tile_sprites +
                                  tile_sprites->first + sizeof(SpriteHeader);
  /* (assumes that first sprite has no palette) */

  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->image +
                                  bytes_across_buf * ((buffer->height + 1) -
                                  SFMapTile_Height / 4);

  assert(base != NULL || overlay != NULL);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      /* Read tile number from overlay or base map? */
      char tile_num;

      if (overlay == NULL) {
        if (base != NULL)
          tile_num = base[row][col]; /* we have no overlay - read tile from basemap */
        else
          tile_num = UINT8_MAX; /* no base map either */
      } else {
        tile_num = overlay[row][col]; /* read tile from map overlay */
        if (base != NULL && tile_num == UINT8_MAX) {
          /* we have both basemap and overlay, overlay is transparent */
          tile_num = base[row][col]; /* read tile from basemap */
        }
      }

      {
        /* Plot tile */
        unsigned char *tile_read_addr = NULL, *line_plot_addr = tile_plot_addr;
        if (tile_num != UINT8_MAX) {
          if (tile_num >= tile_sprites->sprite_count)
            tile_num = 0; /* FIXME: substitute a placeholder sprite? */

          tile_read_addr = first_teximage + tile_num * (sizeof(SpriteHeader) +
                           sizeof(SFMapTile));
          /* (assumes sprites stored consecutively and have no palette/mask) */
        } else
          needs_mask = true;


        if (inv_map != NULL && inv_map[row][col] != 0) {
          if (tile_num != UINT8_MAX) {
            for (int line = 0; line < SFMapTile_Height/4; line++) {
              line_plot_addr[0] = tile_read_addr[0] ^ UCHAR_MAX;
              line_plot_addr[1] = tile_read_addr[4] ^ UCHAR_MAX;
              line_plot_addr[2] = tile_read_addr[8] ^ UCHAR_MAX;
              line_plot_addr[3] = tile_read_addr[12] ^ UCHAR_MAX;

              tile_read_addr += SFMapTile_Width*4;
              line_plot_addr += bytes_across_buf;
            }
          }
        } else {
          if (tile_num != UINT8_MAX) {
            for (int line = 0; line < SFMapTile_Height/4; line++) {
              line_plot_addr[0] = tile_read_addr[0];
              line_plot_addr[1] = tile_read_addr[4];
              line_plot_addr[2] = tile_read_addr[8];
              line_plot_addr[3] = tile_read_addr[12];

              tile_read_addr += SFMapTile_Width*4;
              line_plot_addr += bytes_across_buf;
            }
          }
        }
      }
      tile_plot_addr += SFMapTile_Width/4;
    }
    plot_row_start -= bytes_across_buf * (SFMapTile_Height/4);
  }
  return needs_mask;
}

bool FastPlot_plotareaD(size_t table_size, char tile_colstable[table_size], SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns])
{
  /*  1:16 (1x1) */
  bool needs_mask = false;
  size_t bytes_across_buf = (buffer->width+1)*4;
  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->image +
                                  bytes_across_buf * ((buffer->height + 1) - 1);

  assert(base != NULL || overlay != NULL);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      /* Read tile number from overlay or base map? */
      char tile_num;

      if (overlay == NULL) {
        if (base != NULL)
          tile_num = base[row][col]; /* we have no overlay - read tile from basemap */
        else
          tile_num = UINT8_MAX; /* no base map either */
      } else {
        tile_num = overlay[row][col]; /* read tile from map overlay */
        if (base != NULL && tile_num == UINT8_MAX) {
          /* we have both basemap and overlay, overlay is transparent */
          tile_num = base[row][col]; /* read tile from basemap */
        }
      }

      if (tile_num != UINT8_MAX) {
        if (tile_num >= table_size)
          tile_num = 0; /* FIXME: substitute a placeholder colour? */
      } else
        needs_mask = true;

      /* Plot point of average tile colour */
      if (inv_map != NULL && inv_map[row][col] != 0) {
        if (tile_num != UINT8_MAX)
          *tile_plot_addr = tile_colstable[tile_num] ^ UCHAR_MAX;
      } else {
        if (tile_num != UINT8_MAX)
          *tile_plot_addr = tile_colstable[tile_num];
      }
      tile_plot_addr++;
    }
    plot_row_start -= bytes_across_buf;
  }
  return needs_mask;
}

void FastPlot_plotmask(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns])
{
  /*  1:1 (16x16) */
  size_t bytes_across_buf = (buffer->width+1) * 4;
  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->mask +
                                  (buffer->width + 1) * 4 *
                                  ((buffer->height + 1) - SFMapTile_Height);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      if (overlay[row][col] == UINT8_MAX) {
        /* Plot hole in mask */
        unsigned char *line_plot_addr = tile_plot_addr;
        for (int line = 0; line < SFMapTile_Height; line++) {
          ((unsigned long *)line_plot_addr)[0] = 0;
          ((unsigned long *)line_plot_addr)[1] = 0;
          ((unsigned long *)line_plot_addr)[2] = 0;
          ((unsigned long *)line_plot_addr)[3] = 0;
          line_plot_addr += bytes_across_buf;
        }
      }
      tile_plot_addr += SFMapTile_Width;
    }
    plot_row_start -= bytes_across_buf * SFMapTile_Height;
  }
}

void FastPlot_plotmaskB(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns])
{
  /*  1:2 (8x8) */
  size_t bytes_across_buf = (buffer->width+1) * 4;
  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->mask +
                                  bytes_across_buf * ((buffer->height + 1) -
                                  SFMapTile_Height / 2);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      if (overlay[row][col] == UINT8_MAX) {
        /* Plot hole in mask */
        unsigned char *line_plot_addr = tile_plot_addr;
        for (int line = 0; line < SFMapTile_Height/2; line++) {
          ((unsigned long *)line_plot_addr)[0] = 0;
          ((unsigned long *)line_plot_addr)[1] = 0;
          line_plot_addr += bytes_across_buf;
        }
      }
      tile_plot_addr += SFMapTile_Width/2;
    }
    plot_row_start -= bytes_across_buf * (SFMapTile_Height/2);
  }
}

void FastPlot_plotmaskC(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns])
{
  /*  1:4 (4x4) */
  size_t bytes_across_buf = (buffer->width+1) * 4;
  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->mask +
                                  bytes_across_buf * ((buffer->height + 1) -
                                  SFMapTile_Height / 4);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      if (overlay[row][col] == UINT8_MAX) {
        /* Plot hole in mask */
        unsigned char *line_plot_addr = tile_plot_addr;
        *(unsigned long *)line_plot_addr = 0;
        line_plot_addr += bytes_across_buf;
        *(unsigned long *)line_plot_addr = 0;
        line_plot_addr += bytes_across_buf;
        *(unsigned long *)line_plot_addr = 0;
        line_plot_addr += bytes_across_buf;
        *(unsigned long *)line_plot_addr = 0;
        line_plot_addr += bytes_across_buf;
      }
      tile_plot_addr += SFMapTile_Width/4;
    }
    plot_row_start -= bytes_across_buf * (SFMapTile_Height/4);
  }
}

void FastPlot_plotmaskD(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns])
{
  /*  1:16 (1x1) */
  size_t bytes_across_buf = (buffer->width + 1) * 4;
  unsigned char *plot_row_start = (unsigned char *)buffer + buffer->mask +
                                  bytes_across_buf * ((buffer->height + 1) - 1);

  for (int row = area->ymin; row < area->ymax; row++) {
    unsigned char *tile_plot_addr = plot_row_start;

    for (int col = area->xmin; col < area->xmax; col++) {
      if (overlay[row][col] == UINT8_MAX)
        *tile_plot_addr = 0; /* plot hole in mask */
      tile_plot_addr++;
    }
    plot_row_start -= bytes_across_buf;
  }
}

#endif
