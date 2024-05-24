/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot a specified area of the ground map
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef FastPlot_h
#define FastPlot_h

#include <stdbool.h>
#include <stddef.h>

#include "wimp.h"

#include "SprFormats.h"

bool FastPlot_plotarea(SpriteAreaHeader *tile_sprites, SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns]);
bool FastPlot_plotareaB(SpriteAreaHeader *tile_sprites, SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns]);
bool FastPlot_plotareaC(SpriteAreaHeader *tile_sprites, SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns]);
bool FastPlot_plotareaD(size_t table_size, char tile_colstable[table_size], SpriteHeader *buffer, BBox *area, size_t columns, uint8_t base[][columns], uint8_t overlay[][columns], uint8_t inv_map[][columns]);

void FastPlot_plotmask(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns]);
void FastPlot_plotmaskB(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns]);
void FastPlot_plotmaskC(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns]);
void FastPlot_plotmaskD(SpriteHeader *buffer, BBox *area, size_t columns, uint8_t overlay[][columns]);

/* (See PlotMap.h for functions that take sprite area + name instead of sprite pointer) */

#endif
