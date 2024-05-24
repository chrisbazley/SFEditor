/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map snakes tool implementation
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MSnakes_h
#define MSnakes_h

#include <stdbool.h>

#include "MapCoord.h"
#include "MapEdit.h"
#include "MapEditChg.h"
#include "MapTexBitm.h"
#include "Snakes.h"
#include "MapTexBitm.h"

enum {
  MapSnakesMiniMapHeight = 3,
  MapSnakesMiniMapWidth = 5,
  MapSnakesThumbnailHeight = MapSnakesMiniMapHeight * MapTexSize,
  MapSnakesThumbnailWidth = MapSnakesMiniMapWidth * MapTexSize, /* in pixels */
};

typedef struct MapSnakes MapSnakes;

typedef struct {
  SnakeContext super;
  MapEditContext const *map;
  MapEditChanges *change_info;
} MapSnakesContext;

void MapSnakes_init(MapSnakes *snakes_data);

size_t MapSnakes_get_count(const MapSnakes *snakes_data);

void MapSnakes_get_name(const MapSnakes *snakes_data, size_t snake,
  char *snake_name, size_t n);

bool MapSnakes_ensure_thumbnails(MapSnakes *snakes_data,
  MapTexBitmaps *textures);

void MapSnakes_edit(const char *tiles_set);

void MapSnakes_load(MapSnakes *snakes_data,
  const char *tiles_set, size_t ntiles);

void MapSnakes_begin_line(MapSnakesContext *ctx,
  MapEditContext const *map, /*MapData *s_map,*/
  MapSnakes *snakes_data, MapPoint map_pos, size_t snake,
  bool inside, MapEditChanges *change_info);

void MapSnakes_plot_line(MapSnakesContext *ctx, MapPoint map_pos,
  MapEditChanges *change_info);

void MapSnakes_free(MapSnakes *snakes_data);

#endif
