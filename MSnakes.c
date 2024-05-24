/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map snakes tool implementation
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

/* ANSI headers */
#include <assert.h>
#include <string.h>
#include "stdio.h"
#include <limits.h>
#include "stdlib.h"

/* My headers */
#include "Snakes.h"
#include "SprFormats.h"
#include "hourglass.h"
#include "err.h"
#include "Macros.h"
#include "FileUtils.h"

#include "Session.h"
#include "utils.h"
#include "debug.h"
#include "MSnakes.h"
#include "DrawTiles.h"
#include "MapEdit.h"
#include "MapCoord.h"
#include "MSnakesData.h"
#include "SprMem.h"
#include "FilePaths.h"

/* ---------------- Private functions --------------- */

typedef struct {
  SnakeContext super;
  MapRef (*thumb_tiles)[MapSnakesMiniMapHeight][MapSnakesMiniMapWidth];
} MapSnakesMiniContext;

static size_t read_mini_map(MapPoint const map_pos, SnakeContext *const ctx)
{
  assert(ctx);
  MapSnakesMiniContext const *const mctx = CONTAINER_OF(ctx, MapSnakesMiniContext, super);

  if (map_pos.x < 0 ||
      map_pos.x >= MapSnakesMiniMapWidth ||
      map_pos.y < 0 ||
      map_pos.y >= MapSnakesMiniMapHeight)
  {
    return Map_RefMask;
  }

  return map_ref_to_num((*mctx->thumb_tiles)[map_pos.y][map_pos.x]);
}

static void write_mini_map(MapPoint const map_pos, size_t const tile, SnakeContext *const ctx)
{
  assert(ctx);
  MapSnakesMiniContext const *const mctx = CONTAINER_OF(ctx, MapSnakesMiniContext, super);
  assert(map_pos.x >= 0);
  assert(map_pos.x < MapSnakesMiniMapWidth);
  assert(map_pos.y >= 0);
  assert(map_pos.y < MapSnakesMiniMapHeight);
  (*mctx->thumb_tiles)[map_pos.y][map_pos.x] = map_ref_from_num(tile);
}

static void plot_mini_map(MapSnakesMiniContext *const ctx,
  MapSnakes *const snakes_data, size_t const snake,
  MapPoint const points[], size_t const num_points)
{
  assert(snakes_data);
  assert(points);
  assert(num_points > 0);

  size_t i = 0;
  Snakes_begin_line(&ctx->super, &snakes_data->super, points[i++], snake,
    false, read_mini_map, write_mini_map);

  while (i < num_points) {
    Snakes_plot_line(&ctx->super, points[i++]);
  }
}

static void make_mini_map(MapSnakes *const snakes_data, size_t const snake,
  MapRef (*const thumb_tiles)[MapSnakesMiniMapHeight][MapSnakesMiniMapWidth])
{
  for (int y = 0; y < MapSnakesMiniMapHeight; ++y) {
    for (int x = 0; x < MapSnakesMiniMapWidth; ++x) {
      (*thumb_tiles)[y][x] = map_ref_mask();
    }
  }

  MapSnakesMiniContext ctx = {.thumb_tiles = thumb_tiles};

  if (Snakes_has_bends(&snakes_data->super, snake)) {
    static MapPoint const s_bend[] = {
      {0,                    0},
      {0,                    MapSnakesMiniMapHeight - 1},
      {MapSnakesMiniMapWidth / 2, MapSnakesMiniMapHeight - 1},
      {MapSnakesMiniMapWidth / 2, 0},
      {MapSnakesMiniMapWidth - 1, 0},
      {MapSnakesMiniMapWidth - 1, MapSnakesMiniMapHeight - 1}};

    plot_mini_map(&ctx, snakes_data, snake, s_bend, ARRAY_SIZE(s_bend));
  } else {
    static MapPoint const north_south_cross[] = {
      {MapSnakesMiniMapWidth / 2, MapSnakesMiniMapHeight - 1},
      {MapSnakesMiniMapWidth / 2, 0}};

    plot_mini_map(&ctx, snakes_data, snake, north_south_cross, ARRAY_SIZE(north_south_cross));
  }

  if (Snakes_has_junctions(&snakes_data->super, snake)) {
    static MapPoint const east_west_cross[] = {
      {0,                    MapSnakesMiniMapHeight / 2},
      {MapSnakesMiniMapWidth - 1, MapSnakesMiniMapHeight / 2}};

    plot_mini_map(&ctx, snakes_data, snake, east_west_cross, ARRAY_SIZE(east_west_cross));
  }
}

static DrawTilesReadResult read_thumbnail(void *const cb_arg, MapPoint const map_pos)
{
  const MapRef (*const thumb_tiles)[MapSnakesMiniMapHeight][MapSnakesMiniMapWidth] = cb_arg;
  assert(thumb_tiles != NULL);
  assert(map_pos.x >= 0);
  assert(map_pos.x < MapSnakesMiniMapWidth);
  assert(map_pos.y >= 0);
  assert(map_pos.y < MapSnakesMiniMapHeight);

  return (DrawTilesReadResult){(*thumb_tiles)[map_pos.y][map_pos.x]};
}

static bool make_thumbnails(MapSnakes *const snakes_data,
  MapTexBitmaps *const textures)
{
  assert(snakes_data);
  assert(textures);

  hourglass_on();
  for (size_t snake = 0; snake < snakes_data->super.count; snake++) {

    if (snake <= INT_MAX / 100)
      hourglass_percentage((int)((snake * 100) / snakes_data->super.count));

    /* Create thumbnail sprite */
    char sprite_name[12];
    sprintf(sprite_name, "%zu", snake);

    static Vertex const thumbnail_size = {
      MapSnakesThumbnailWidth, MapSnakesThumbnailHeight
    };
    if (!SprMem_create_sprite(&snakes_data->thumbnail_sprites, sprite_name,
        false, thumbnail_size, DrawTilesModeNumber))
      return false;

    MapArea const scr_area = {{0,0}, {MapSnakesMiniMapWidth - 1, MapSnakesMiniMapHeight - 1}};

    MapRef thumb_tiles[MapSnakesMiniMapHeight][MapSnakesMiniMapWidth];
    make_mini_map(snakes_data, snake, &thumb_tiles);

    bool const needs_mask = DrawTiles_to_sprite(
      textures, &snakes_data->thumbnail_sprites, sprite_name, MapAngle_North, &scr_area,
      read_thumbnail, &thumb_tiles,
      0, /* plot at 1:1 */
      NULL /* no colour translation */
    );

    /* Create thumbnail mask (with all pixels solid) */
    if (needs_mask &&
        SprMem_create_mask(&snakes_data->thumbnail_sprites, sprite_name))
    {
      DrawTiles_to_mask(&snakes_data->thumbnail_sprites, sprite_name, MapAngle_North, &scr_area,
                       read_thumbnail, &thumb_tiles, 0);
    }
  }
  hourglass_off();

  SprMem_minimize(&snakes_data->thumbnail_sprites);

#ifndef NDEBUG
  SprMem_verify(&snakes_data->thumbnail_sprites);

  if (snakes_data->super.count > 0)
    SprMem_save(&snakes_data->thumbnail_sprites, "thumbnail_sprites");
#endif

  return true;
}

/* ---------------- Public functions ---------------- */

size_t MapSnakes_get_count(const MapSnakes *const snakes_data)
{
  return Snakes_get_count(&snakes_data->super);
}

void MapSnakes_get_name(const MapSnakes *const snakes_data, size_t const snake,
  char *const snake_name, size_t const n)
{
  Snakes_get_name(&snakes_data->super, snake, snake_name, n);
}

static size_t read_map(MapPoint const map_pos, SnakeContext *const ctx)
{
  MapSnakesContext const *const mctx = CONTAINER_OF(ctx, MapSnakesContext, super);
  return map_ref_to_num(MapEdit_read_tile(mctx->map, map_pos));
}

static void write_map(MapPoint const map_pos, size_t const tile, SnakeContext *const ctx)
{
  MapSnakesContext const *const mctx = CONTAINER_OF(ctx, MapSnakesContext, super);
  MapEdit_write_tile(mctx->map, map_pos, map_ref_from_num(tile), mctx->change_info);
}

void MapSnakes_begin_line(MapSnakesContext *const ctx,
  MapEditContext const *const map,
  MapSnakes *const snakes_data, MapPoint const map_pos, size_t const snake,
  bool const inside, MapEditChanges *const change_info)
{
  assert(ctx != NULL);
  *ctx = (MapSnakesContext){
    .map = map,
    .change_info = change_info,
  };
  Snakes_begin_line(&ctx->super, &snakes_data->super, map_pos, snake, inside,
                    read_map, write_map);
}

void MapSnakes_plot_line(MapSnakesContext *const ctx, MapPoint const end,
  MapEditChanges *const change_info)
{
  assert(ctx != NULL);
  ctx->change_info = change_info;
  Snakes_plot_line(&ctx->super, end);
}

bool MapSnakes_ensure_thumbnails(MapSnakes *const snakes_data,
  MapTexBitmaps *const textures)
{
  /* N.B. Although we are lazy about creating the thumbnail sprites, we still
     want to be able to share them with all other Session using this tile set.
     Therefore they are part of the _MapSnakes structure */

  /* Make thumbnail sprites for snakes palette windows */
  if (snakes_data->have_thumbnails)
    return true; /* We already have thumbnail sprites */

  DEBUG("Creating thumbnails of snakes for tile set %p", (void *)snakes_data);
  if (!SprMem_init(&snakes_data->thumbnail_sprites, 0))
    return false;

  bool const success = make_thumbnails(snakes_data, textures);
  if (!success) {
    SprMem_destroy(&snakes_data->thumbnail_sprites);
  } else {
    snakes_data->have_thumbnails = true;
  }
  return success;
}

void MapSnakes_edit(char const *const tiles_set)
{
  /* If necessary then copy the default snakes definition file prior to
     opening it for editing */
  edit_file(TILESNAKES_DIR, tiles_set);
}

void MapSnakes_init(MapSnakes *const snakes_data)
{
  assert(snakes_data);
  *snakes_data = (MapSnakes){.have_thumbnails = false};
  Snakes_init(&snakes_data->super);
}

void MapSnakes_load(MapSnakes *const snakes_data,
  char const *const tiles_set, size_t const ntiles)
{
  MapSnakes_free(snakes_data);
  MapSnakes_init(snakes_data);

  char *const full_path = make_file_path_in_dir(
    CHOICES_READ_PATH TILESNAKES_DIR, tiles_set);

  if (!full_path) {
    report_error(SFERROR(NoMem), "", "");
    return;
  }

  char err_buf[ErrBufferSize] = "";
  SFError err = SFERROR(OK);

  hourglass_on();
  if (file_exists(full_path)) {
    FILE *const file = fopen(full_path, "r");
    if (file == NULL) {
      err = SFERROR(OpenInFail);
    } else {
      err = Snakes_load(file, &snakes_data->super, ntiles, err_buf);
      fclose(file);
    }
  }
  hourglass_off();

  report_error(err, full_path, err_buf);
  free(full_path);
}

void MapSnakes_free(MapSnakes *const snakes_data)
{
  assert(snakes_data != NULL);
  Snakes_free(&snakes_data->super);
  if (snakes_data->have_thumbnails)
  {
    SprMem_destroy(&snakes_data->thumbnail_sprites);
  }
}
