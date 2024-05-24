/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map tile set
 *  Copyright (C) 2007 Christopher Bazley
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
#include "stdlib.h"
#include <assert.h>

/* My headers */
#include "debug.h"
#include "Macros.h"
#include "Reader.h"
#include "PathTail.h"
#include "StrDict.h"

#include "MSnakes.h"
#include "MapTexBitm.h"
#include "Smooth.h"
#include "MTransfers.h"
#include "MapTex.h"
#include "MapTexData.h"

static StrDict file_dict;

static void init_all(MapTex *const textures)
{
  assert(textures);

  MapTexBitmaps_init(&textures->tiles);
  MapTexGroups_init(&textures->groups);
  MapSnakes_init(&textures->snakes);
  MapTransfers_init(&textures->transfers);
}

static void destroy_all(MapTex *const textures)
{
  assert(textures);

  MapTexBitmaps_free(&textures->tiles);
  MapTexGroups_free(&textures->groups);
  MapSnakes_free(&textures->snakes);
  MapTransfers_free(&textures->transfers);
}

static SFError MapTex_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  MapTex *const textures = CONTAINER_OF(dfile, MapTex, dfile);

  destroy_all(textures);
  init_all(textures);

  return MapTexBitmaps_read(&textures->tiles, reader);
}

static void MapTex_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  MapTex *const textures = CONTAINER_OF(dfile, MapTex, dfile);

  destroy_all(textures);
  dfile_destroy(&textures->dfile);
  free(textures);
}

static void MapTex_cleanup(void)
{
  strdict_destroy(&file_dict, NULL, NULL);
}

void MapTex_init(void)
{
  strdict_init(&file_dict);
  atexit(MapTex_cleanup);
}

DFile *MapTex_get_dfile(MapTex *const textures)
{
  assert(textures);
  return &textures->dfile;
}

MapTex *MapTex_create(void)
{
  MapTex *const textures = malloc(sizeof(*textures));
  if (textures)
  {
    *textures = (MapTex){{0}};

    dfile_init(&textures->dfile, MapTex_read_cb, NULL, NULL,
               MapTex_destroy_cb);

    init_all(textures);
  }
  return textures;
}

void MapTex_load_metadata(MapTex *const textures)
{
  assert(textures);

  char const *const filename = dfile_get_name(&textures->dfile);
  if (filename == NULL) {
    return;
  }

  char *const leaf_name = pathtail(filename, 1);

  MapTransfers_load_all(&textures->transfers, leaf_name);

  MapTexGroups_load(&textures->groups, leaf_name,
                    MapTexBitmaps_get_count(&textures->tiles));

  MapSnakes_load(&textures->snakes, leaf_name,
                    MapTexBitmaps_get_count(&textures->tiles));
}

bool MapTex_share(MapTex *const textures)
{
  assert(textures);
  return dfile_set_shared(&textures->dfile, &file_dict);
}

MapTex *MapTex_get_shared(char const *const filename)
{
  DFile *const dfile = dfile_find_shared(&file_dict, filename);
  return dfile ? CONTAINER_OF(dfile, MapTex, dfile) : NULL;
}
