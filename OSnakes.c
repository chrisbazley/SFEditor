/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object snakes tool implementation
 *  Copyright (C) 2021 Christopher Bazley
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
#include <stdint.h>

/* My headers */
#include "Snakes.h"
#include "hourglass.h"
#include "err.h"
#include "Macros.h"
#include "FileUtils.h"

#include "Session.h"
#include "utils.h"
#include "debug.h"
#include "OSnakes.h"
#include "OSnakesData.h"
#include "ObjectsEdit.h"
#include "MapCoord.h"
#include "FilePaths.h"
#include "Session.h"
#include "ObjEditChg.h"

static size_t read_map(MapPoint const map_pos, SnakeContext *const ctx)
{
  ObjSnakesContext const *const octx = CONTAINER_OF(ctx, ObjSnakesContext, super);
  return objects_ref_to_num(ObjectsEdit_read_ref(octx->objects, map_pos));
}

static void write_map(MapPoint const map_pos, size_t const ref_num, SnakeContext *const ctx)
{
  ObjSnakesContext const *const octx = CONTAINER_OF(ctx, ObjSnakesContext, super);

  ObjectsEdit_write_ref(octx->objects, map_pos, objects_ref_from_num(ref_num),
    TriggersWipeAction_BreakChain, octx->change_info, octx->meshes);
}

/* ---------------- Public functions ---------------- */

size_t ObjSnakes_get_count(const ObjSnakes *const snakes_data)
{
  assert(snakes_data);
  return Snakes_get_count(&snakes_data->super);
}

void ObjSnakes_get_name(const ObjSnakes *const snakes_data, size_t const snake,
  char *const snake_name, size_t const n)
{
  assert(snakes_data);
  Snakes_get_name(&snakes_data->super, snake, snake_name, n);
}

long int ObjSnakes_get_pal_distance(const ObjSnakes *const snakes_data,
  size_t const snake)
{
  assert(snakes_data);
  assert(snake >= 0);
  assert(snake < Snakes_get_count(&snakes_data->super));

  return snakes_data->distances ? snakes_data->distances[snake] : -1;
}

void ObjSnakes_set_pal_distance(ObjSnakes *const snakes_data,
  size_t const snake, long int const distance)
{
  assert(snakes_data);
  assert(snake >= 0);
  assert(distance >= 0);

  size_t const count = Snakes_get_count(&snakes_data->super);
  assert(snake < count);

  if (!snakes_data->distances) {
    snakes_data->distances = malloc(sizeof(*snakes_data->distances) * (size_t)count);

    if (!snakes_data->distances) {
      report_error(SFERROR(NoMem), "", "");
      return;
    }

    for (size_t i = 0; i < count; ++i) {
      snakes_data->distances[i] = -1;
    }
  }
  snakes_data->distances[snake] = distance;
}

ObjRef ObjSnakes_get_value(EditSession *const session,
  ObjSnakes *const snakes_data, MapPoint const map_pos, size_t const snake,
  bool const inside, ObjGfxMeshes *const meshes)
{
  assert(snakes_data != NULL);
  ObjSnakesContext ctx = {
    .session = session,
    .objects = Session_get_objects(session),
    .meshes = meshes,
  };
  return objects_ref_from_num(
    Snakes_begin_line(&ctx.super, &snakes_data->super, map_pos, snake, inside,
                           read_map, NULL));
}

void ObjSnakes_begin_line(ObjSnakesContext *const ctx,
  EditSession *const session,
  ObjSnakes *const snakes_data, MapPoint const map_pos, size_t const snake,
  bool const inside, ObjEditChanges *const change_info,
  ObjGfxMeshes *const meshes)
{
  assert(ctx != NULL);
  assert(snakes_data != NULL);
  *ctx = (ObjSnakesContext){
    .session = session,
    .objects = Session_get_objects(session),
    .change_info = change_info,
    .meshes = meshes,
  };
  Snakes_begin_line(&ctx->super, &snakes_data->super, map_pos, snake, inside,
                    read_map, write_map);
}

void ObjSnakes_plot_line(ObjSnakesContext *const ctx, MapPoint const end,
  ObjEditChanges *const change_info)
{
  assert(ctx != NULL);
  ctx->change_info = change_info;
  Snakes_plot_line(&ctx->super, end);
}

void ObjSnakes_edit(char const *const tiles_set)
{
  /* If necessary then copy the default snakes definition file prior to
     opening it for editing */
  edit_file(OBJSNAKES_DIR, tiles_set);
}

void ObjSnakes_init(ObjSnakes *const snakes_data)
{
  assert(snakes_data);
  *snakes_data = (ObjSnakes){{0}};
  Snakes_init(&snakes_data->super);
}

void ObjSnakes_load(ObjSnakes *const snakes_data,
  char const *const tiles_set, size_t const nobj)
{
  ObjSnakes_free(snakes_data);
  ObjSnakes_init(snakes_data);

  char *const full_path = make_file_path_in_dir(
    CHOICES_READ_PATH OBJSNAKES_DIR, tiles_set);

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
      err = Snakes_load(file, &snakes_data->super, nobj, err_buf);
      fclose(file);
    }
  }
  hourglass_off();

  report_error(err, full_path, err_buf);
  free(full_path);
}

void ObjSnakes_free(ObjSnakes *const snakes_data)
{
  assert(snakes_data != NULL);
  Snakes_free(&snakes_data->super);
  free(snakes_data->distances);
}
